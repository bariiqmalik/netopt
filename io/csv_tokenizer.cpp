#include "io/csv_tokenizer.hpp"
#include <cctype>

namespace netopt {

std::string CsvTokenizer::trim(std::string_view str) {
    size_t first = 0;
    while (first < str.size() && (str[first] == ' ' || str[first] == '\t' || str[first] == '\r')) {
        ++first;
    }
    if (first == str.size()) {
        return "";
    }
    size_t last = str.size() - 1;
    while (last > first && (str[last] == ' ' || str[last] == '\t' || str[last] == '\r')) {
        --last;
    }
    return std::string(str.substr(first, last - first + 1));
}

Result<std::vector<CsvRow>> CsvTokenizer::tokenize(std::string_view content,
                                                  const std::string& filename,
                                                  DiagnosticsCollector& diagnostics) {
    std::vector<CsvRow> rows;

    // Check for UTF-8 BOM (\xEF\xBB\xBF)
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.remove_prefix(3);
    }

    size_t i = 0;
    const size_t n = content.size();
    size_t currentLine = 1;

    while (i < n) {
        // Skip leading carriage returns or line feeds if outside a row
        if (content[i] == '\r') {
            ++i;
            if (i < n && content[i] == '\n') {
                ++i;
            }
            ++currentLine;
            continue;
        }
        if (content[i] == '\n') {
            ++i;
            ++currentLine;
            continue;
        }

        const size_t rowStartLine = currentLine;

        // Check if this line is a comment line starting with '#'
        // Find if line starts with '#' (allowing leading whitespace)
        size_t temp = i;
        while (temp < n && (content[temp] == ' ' || content[temp] == '\t')) {
            ++temp;
        }
        if (temp < n && content[temp] == '#') {
            size_t lineEnd = temp;
            while (lineEnd < n && content[lineEnd] != '\n' && content[lineEnd] != '\r') {
                ++lineEnd;
            }
            std::string commentStr = trim(content.substr(temp, lineEnd - temp));
            rows.push_back(CsvRow{
                .lineNumber = rowStartLine,
                .fields = {},
                .isComment = true,
                .commentText = std::move(commentStr)
            });
            i = lineEnd;
            continue;
        }

        // Parse fields of this row
        std::vector<std::string> fields;
        bool inRow = true;

        while (inRow && i <= n) {
            // Skip leading whitespace before field if not quoted
            while (i < n && (content[i] == ' ' || content[i] == '\t')) {
                ++i;
            }

            if (i < n && content[i] == '"') {
                // Quoted field
                ++i; // skip initial quote
                std::string field;
                bool closed = false;

                while (i < n) {
                    if (content[i] == '"') {
                        if (i + 1 < n && content[i + 1] == '"') {
                            field.push_back('"');
                            i += 2;
                        } else {
                            closed = true;
                            ++i;
                            break;
                        }
                    } else {
                        if (content[i] == '\n') {
                            ++currentLine;
                        }
                        field.push_back(content[i]);
                        ++i;
                    }
                }

                if (!closed) {
                    diagnostics.addError(filename, rowStartLine, "Unclosed quote in CSV field");
                }

                // Skip any trailing spaces after closing quote until comma or newline
                while (i < n && (content[i] == ' ' || content[i] == '\t')) {
                    ++i;
                }

                fields.push_back(std::move(field));

                if (i < n && content[i] == ',') {
                    ++i; // consume comma, next field
                } else if (i >= n || content[i] == '\n' || content[i] == '\r') {
                    inRow = false;
                } else {
                    diagnostics.addWarning(filename, currentLine, "Unexpected character after quoted field");
                }
            } else {
                // Unquoted field
                size_t fieldStart = i;
                while (i < n && content[i] != ',' && content[i] != '\n' && content[i] != '\r') {
                    ++i;
                }
                std::string field = trim(content.substr(fieldStart, i - fieldStart));
                fields.push_back(std::move(field));

                if (i < n && content[i] == ',') {
                    ++i; // consume comma, next field
                    // If comma is right at end of line/file, add an empty field
                    if (i >= n || content[i] == '\n' || content[i] == '\r') {
                        fields.push_back("");
                        inRow = false;
                    }
                } else {
                    inRow = false;
                }
            }
        }

        // Ignore empty lines with single empty field
        if (fields.size() == 1 && fields[0].empty()) {
            continue;
        }

        rows.push_back(CsvRow{
            .lineNumber = rowStartLine,
            .fields = std::move(fields),
            .isComment = false,
            .commentText = ""
        });
    }

    return Result<std::vector<CsvRow>>::ok(std::move(rows));
}

} // namespace netopt
