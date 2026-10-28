#include "parser.hpp"

#include <charconv>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>

namespace rank_queries {

namespace {

template <typename Number> std::optional<Number> to_number(const std::string& token) {
    Number value{};
    const char* const end = token.data() + token.size();
    const auto [stop, error] = std::from_chars(token.data(), end, value);
    if (error != std::errc{} || stop != end) {
        return std::nullopt;
    }
    return value;
}

template <typename Number> std::optional<Number> read_number(std::istream& input, std::string& text) {
    std::string token;
    if (!(input >> token)) {
        return std::nullopt;
    }
    text += ' ';
    text += token;
    return to_number<Number>(token);
}

} // namespace

ParseResult parse(std::istream& input, std::ostream& errors) {
    ParseResult result;
    const auto report = [&result, &errors](const std::string& message) {
        errors << "Error: " << message << '\n';
        ++result.error_count;
    };

    std::string command;
    while (input >> command) {
        if (command.size() != 1) {
            report("Unknown command '" + command + "'");
            continue;
        }

        std::string text = command;
        std::optional<Query> query;
        switch (command[0]) {
            case 'k':
                if (const auto key = read_number<Key>(input, text)) {
                    query = Insert{*key};
                }
                break;

            case 'n':
                if (const auto key = read_number<Key>(input, text)) {
                    query = CountLess{*key};
                }
                break;

            case 'q': {
                const auto low = read_number<Key>(input, text);
                const auto high = read_number<Key>(input, text);
                if (low && high) {
                    query = CountRange{*low, *high};
                }
                break;
            }

            case 'm':
                if (const auto index = read_number<std::size_t>(input, text)) {
                    query = KthSmallest{*index};
                }
                break;

            default:
                report("Unknown command '" + command + "'");
                continue;
        }

        if (query) {
            result.queries.push_back(*query);
        } else if (input) {
            report("Invalid query '" + text + "'");
        } else {
            report("Missing argument in '" + text + "'");
        }
    }
    return result;
}

} // namespace rank_queries
