#include "output.hpp"

#include <ostream>
#include <variant>

namespace rank_queries {

namespace {

struct Writer {
    std::ostream& out;
    std::ostream& errors;
    bool written_any = false;
    std::size_t error_count = 0;

    void operator()(const CountAnswer& answer) {
        write(answer.value);
    }

    void operator()(const KeyAnswer& answer) {
        write(answer.value);
    }

    void operator()(const OutOfRange& answer) {
        errors << "Error: Index " << answer.index << " is out of range\n";
        ++error_count;
    }

    template <typename Number> void write(Number value) {
        if (written_any) {
            out << ' ';
        }
        out << value;
        written_any = true;
    }
};

} // namespace

std::size_t output(const std::vector<Answer>& answers, std::ostream& out, std::ostream& errors) {
    Writer writer{out, errors};
    for (const auto& answer : answers) {
        std::visit(writer, answer);
    }
    if (writer.written_any) {
        out << '\n';
    }
    return writer.error_count;
}

} // namespace rank_queries
