#include "query.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <random>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using rank_queries::Key;

constexpr long long key_range = 1LL << 32;
constexpr long long key_min = -(1LL << 31);

struct Settings {
    std::size_t keys = 0;
    std::size_t ops = 0;
    std::size_t insert_percent = 0;
    double range_percent = 1.0;
    std::size_t clusters = 100;
    std::uint64_t seed = 1;
    std::string_view order = "random";
    std::string_view queries = "mixed";
};

template <typename Number> bool parse_number(std::string_view text, Number& value) {
    const char* const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return (error == std::errc{}) && (stop == end);
}

struct Option {
    std::string_view prefix;
    bool (*parse)(std::string_view value, Settings& settings);
};

constexpr Option options[] = {
    {"--keys=",           [](std::string_view value, Settings& settings) { return parse_number(value, settings.keys); }},
    {"--ops=",            [](std::string_view value, Settings& settings) { return parse_number(value, settings.ops); }},
    {"--insert-percent=", [](std::string_view value, Settings& settings) { return parse_number(value, settings.insert_percent); }},
    {"--range-percent=",  [](std::string_view value, Settings& settings) { return parse_number(value, settings.range_percent); }},
    {"--clusters=",       [](std::string_view value, Settings& settings) { return parse_number(value, settings.clusters); }},
    {"--seed=",           [](std::string_view value, Settings& settings) { return parse_number(value, settings.seed); }},
    {"--order=",          [](std::string_view value, Settings& settings) { settings.order = value; return true; }},
    {"--queries=",        [](std::string_view value, Settings& settings) { settings.queries = value; return true; }},
};

bool parse_argument(std::string_view argument, Settings& settings) {
    for (const auto& option : options) {
        if (argument.starts_with(option.prefix)) {
            return option.parse(argument.substr(option.prefix.size()), settings);
        }
    }
    return false;
}

bool is_valid(const Settings& settings) {
    const bool known_order = (settings.order == "random") || (settings.order == "ascending") ||
                             (settings.order == "descending") || (settings.order == "clustered");
    const bool known_queries = (settings.queries == "mixed") || (settings.queries == "n") ||
                               (settings.queries == "m") || (settings.queries == "q");
    const auto limit = static_cast<std::size_t>(key_range);
    return known_order && known_queries && (settings.keys >= 1) && (settings.keys <= limit) && (settings.ops <= limit) &&
           (settings.keys + settings.ops <= limit) && (settings.insert_percent <= 100) && (settings.range_percent > 0.0) &&
           (settings.range_percent <= 100.0) && (settings.clusters >= 1);
}

std::vector<Key> make_sorted_keys(std::size_t count) {
    const long long step = key_range / static_cast<long long>(count);
    std::vector<Key> keys(count);
    for (std::size_t i = 0; i < count; ++i) {
        keys[i] = static_cast<Key>(key_min + static_cast<long long>(i) * step);
    }
    return keys;
}

std::vector<Key> make_clustered(const std::vector<Key>& sorted, std::size_t clusters, std::mt19937_64& generator) {
    std::vector<std::size_t> cluster_order(clusters);
    std::iota(cluster_order.begin(), cluster_order.end(), std::size_t{0});
    std::shuffle(cluster_order.begin(), cluster_order.end(), generator);

    std::vector<Key> arrivals;
    arrivals.reserve(sorted.size());
    for (const std::size_t cluster : cluster_order) {
        const auto first = sorted.begin() + static_cast<std::ptrdiff_t>(cluster * sorted.size() / clusters);
        const auto last = sorted.begin() + static_cast<std::ptrdiff_t>((cluster + 1) * sorted.size() / clusters);
        arrivals.insert(arrivals.end(), first, last);
    }
    return arrivals;
}

std::vector<Key> make_arrivals(const std::vector<Key>& sorted, const Settings& settings, std::mt19937_64& generator) {
    if (settings.order == "clustered") {
        return make_clustered(sorted, settings.clusters, generator);
    }
    std::vector<Key> arrivals = sorted;
    if (settings.order == "random") {
        std::shuffle(arrivals.begin(), arrivals.end(), generator);
    } else if (settings.order == "descending") {
        std::reverse(arrivals.begin(), arrivals.end());
    }
    return arrivals;
}

std::vector<char> make_operation_kinds(const Settings& settings, std::size_t inserts, std::mt19937_64& generator) {
    std::vector<char> kinds(inserts, 'k');
    kinds.reserve(settings.ops);
    for (std::size_t i = 0; i < settings.ops - inserts; ++i) {
        kinds.push_back(settings.queries == "mixed" ? "nmq"[i % 3] : settings.queries[0]);
    }
    std::shuffle(kinds.begin(), kinds.end(), generator);
    return kinds;
}

void generate(const Settings& settings, std::ostream& output) {
    std::mt19937_64 generator{settings.seed};
    const std::size_t inserts = settings.ops * settings.insert_percent / 100;
    const std::size_t total = settings.keys + inserts;

    const std::vector<Key> sorted = make_sorted_keys(total);
    const std::vector<Key> arrivals = make_arrivals(sorted, settings, generator);
    const std::vector<char> kinds = make_operation_kinds(settings, inserts, generator);

    const auto rounded_width = static_cast<std::size_t>(std::llround(settings.range_percent / 100.0 * static_cast<double>(total)));
    const std::size_t width = std::min(std::max<std::size_t>(rounded_width, 1), total - 1);
    const auto pick = [&generator](std::size_t low, std::size_t high) {
        return std::uniform_int_distribution<std::size_t>{low, high}(generator);
    };

    std::size_t size = settings.keys;
    for (std::size_t i = 0; i < size; ++i) {
        output << "k " << arrivals[i] << '\n';
    }
    for (const char kind : kinds) {
        switch (kind) {
            case 'k':
                output << "k " << arrivals[size++] << '\n';
                break;

            case 'n':
                output << "n " << sorted[pick(0, total - 1)] << '\n';
                break;

            case 'm':
                output << "m " << pick(1, size) << '\n';
                break;

            case 'q': {
                const std::size_t low = pick(0, total - 1 - width);
                output << "q " << sorted[low] << ' ' << sorted[low + width] << '\n';
                break;
            }
        }
    }
}

void print_usage(int argc, char* argv[]) {
    std::cerr << "Usage: ";
    if (argc > 0) {
        std::cerr << std::filesystem::path(argv[0]).filename().string() << ' ';
    }
    std::cerr << "--keys=N [--ops=M] [--insert-percent=P] [--order=random|ascending|descending|clustered] "
                 "[--clusters=C] [--queries=mixed|n|m|q] [--range-percent=W] [--seed=S]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    Settings settings;
    for (int i = 1; i < argc; ++i) {
        if (!parse_argument(argv[i], settings)) {
            print_usage(argc, argv);
            return 1;
        }
    }
    if (!is_valid(settings)) {
        print_usage(argc, argv);
        return 1;
    }

    generate(settings, std::cout);
    return 0;
}
