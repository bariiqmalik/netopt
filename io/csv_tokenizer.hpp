#pragma once

#include "core/types.hpp"
#include "io/diagnostics.hpp"
#include <string>
#include <vector>
#include <string_view>

namespace netopt {

struct CsvRow {
    size_t lineNumber{0};
    std::vector<std::string> fields;
    bool isComment{false};
    std::string commentText;
};

class CsvTokenizer {
public:
    static Result<std::vector<CsvRow>> tokenize(std::string_view content,
                                               const std::string& filename,
                                               DiagnosticsCollector& diagnostics);

private:
    static std::string trim(std::string_view str);
};

} // namespace netopt
