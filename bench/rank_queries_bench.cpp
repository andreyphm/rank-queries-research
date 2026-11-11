#include "output.hpp"
#include "parser.hpp"
#include "runner.hpp"
#include "structures/avl_tree.hpp"
#include "structures/btree.hpp"
#include "structures/set_wrapper.hpp"
#include "structures/sorted_vector.hpp"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using rank_queries::Key;
using rank_queries::Query;

struct Measurement {
    long long setup_ns;
    long long ops_ns;
    std::vector<rank_queries::Answer> setup_answers;
    std::vector<rank_queries::Answer> ops_answers;
};

using MeasureFunc = Measurement (*)(const std::vector<Query>&, const std::vector<Query>&);

template <typename Set> Measurement measure(const std::vector<Query>& setup, const std::vector<Query>& ops) {
    using Clock = std::chrono::steady_clock;
    const auto to_ns = [](Clock::duration duration) {
        return static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
    };

    Set set;
    const auto start = Clock::now();
    std::vector<rank_queries::Answer> setup_answers = rank_queries::run(set, setup);
    const auto setup_end = Clock::now();
    std::vector<rank_queries::Answer> ops_answers = rank_queries::run(set, ops);
    const auto ops_end = Clock::now();

    return {to_ns(setup_end - start), to_ns(ops_end - setup_end), std::move(setup_answers), std::move(ops_answers)};
}

template <std::size_t T> constexpr MeasureFunc measure_btree = measure<rank_queries::BTree<Key, std::less<Key>, T>>;

struct Impl {
    std::string_view name;
    MeasureFunc measure_func;
};

constexpr Impl impls[] = {
    {"vector",   measure<rank_queries::SortedVector<Key>>},
    {"set",      measure<rank_queries::SetWrapper<Key>>},
    {"avl",      measure<rank_queries::AvlTree<Key>>},
    {"btree8",   measure_btree<8>},
    {"btree16",  measure_btree<16>},
    {"btree32",  measure_btree<32>},
    {"btree64",  measure_btree<64>},
    {"btree128", measure_btree<128>},
};

constexpr std::string_view impl_prefix = "--impl=";
constexpr std::string_view setup_count_prefix = "--setup-count=";
constexpr std::string_view print_answers_flag = "--print-answers";

struct Settings {
    MeasureFunc measure_func;
    std::size_t setup_count;
    bool print_answers;
};

std::optional<MeasureFunc> find_measure_func(std::string_view name) {
    for (const auto& impl : impls) {
        if (impl.name == name) {
            return impl.measure_func;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> parse_count(std::string_view text) {
    std::size_t value = 0;
    const char* const end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || stop != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<Settings> parse_settings(int argc, char* argv[]) {
    std::optional<MeasureFunc> measure_func;
    std::optional<std::size_t> setup_count;
    bool print_answers = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with(impl_prefix)) {
            measure_func = find_measure_func(argument.substr(impl_prefix.size()));
        } else if (argument.starts_with(setup_count_prefix)) {
            setup_count = parse_count(argument.substr(setup_count_prefix.size()));
        } else if (argument == print_answers_flag) {
            print_answers = true;
        } else {
            return std::nullopt;
        }
    }
    if (!measure_func || !setup_count) {
        return std::nullopt;
    }
    return Settings{*measure_func, *setup_count, print_answers};
}

void print_usage(int argc, char* argv[]) {
    std::cerr << "Usage: ";
    if (argc > 0) {
        std::cerr << std::filesystem::path(argv[0]).filename().string() << ' ';
    }
    std::cerr << impl_prefix;
    for (std::size_t i = 0; i < std::size(impls); ++i) {
        if (i > 0) {
            std::cerr << '|';
        }
        std::cerr << impls[i].name;
    }
    std::cerr << ' ' << setup_count_prefix << "K [" << print_answers_flag << "]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    const std::optional<Settings> settings = parse_settings(argc, argv);
    if (!settings) {
        print_usage(argc, argv);
        return 1;
    }

    const rank_queries::ParseResult parsed = rank_queries::parse(std::cin, std::cerr);
    if (parsed.error_count != 0) {
        return 1;
    }
    if (settings->setup_count > parsed.queries.size()) {
        std::cerr << "Error: Setup count " << settings->setup_count << " exceeds " << parsed.queries.size()
                  << " commands\n";
        return 1;
    }

    const auto setup_end = parsed.queries.begin() + static_cast<std::ptrdiff_t>(settings->setup_count);
    const std::vector<Query> setup(parsed.queries.begin(), setup_end);
    const std::vector<Query> ops(setup_end, parsed.queries.end());

    const Measurement measurement = settings->measure_func(setup, ops);
    std::cout << measurement.setup_ns << ',' << measurement.ops_ns << '\n';
    if (!settings->print_answers) {
        return 0;
    }

    std::cout << "setup answers:\n";
    const std::size_t setup_errors = rank_queries::output(measurement.setup_answers, std::cout, std::cerr);
    std::cout << "ops answers:\n";
    const std::size_t ops_errors = rank_queries::output(measurement.ops_answers, std::cout, std::cerr);
    return (setup_errors + ops_errors == 0) ? 0 : 1;
}
