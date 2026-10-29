#include "output.hpp"
#include "parser.hpp"
#include "runner.hpp"
#include "set_wrapper.hpp"
#include "sorted_vector.hpp"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {

using RunFunc = std::vector<rank_queries::Answer> (*)(const std::vector<rank_queries::Query>&);

struct Impl {
    std::string_view name;
    RunFunc run_func;
};

constexpr Impl impls[] = {
    {"vector",  rank_queries::run<rank_queries::SortedVector<rank_queries::Key>>},
    {"set",     rank_queries::run<rank_queries::SetWrapper<rank_queries::Key>>},
};

constexpr std::string_view default_impl = "set";
constexpr std::string_view impl_prefix = "--impl=";

std::optional<RunFunc> select_run_func(int argc, char* argv[]) {
    if (argc > 2) {
        return std::nullopt;
    }

    std::string_view name = default_impl;
    if (argc == 2) {
        const std::string_view argument = argv[1];
        if (!argument.starts_with(impl_prefix)) {
            return std::nullopt;
        }
        name = argument.substr(impl_prefix.size());
    }

    for (const auto& impl : impls) {
        if (impl.name == name) {
            return impl.run_func;
        }
    }
    return std::nullopt;
}

void print_usage(int argc, char* argv[]) {
    std::cerr << "Usage: ";
    if (argc > 0) {
        std::cerr << std::filesystem::path(argv[0]).filename().string() << ' ';
    }
    std::cerr << '[' << impl_prefix;
    for (std::size_t i = 0; i < std::size(impls); ++i) {
        if (i > 0) {
            std::cerr << '|';
        }
        std::cerr << impls[i].name;
    }
    std::cerr << "]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    const auto run_func = select_run_func(argc, argv);
    if (!run_func) {
        print_usage(argc, argv);
        return 1;
    }

    const auto parsed = rank_queries::parse(std::cin, std::cerr);
    const auto answers = (*run_func)(parsed.queries);
    const auto output_error_count = rank_queries::output(answers, std::cout, std::cerr);

    return (parsed.error_count + output_error_count == 0) ? 0 : 1;
}
