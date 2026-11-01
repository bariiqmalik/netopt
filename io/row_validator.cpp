#include "io/row_validator.hpp"
#include <charconv>
#include <cstdlib>
#include <cmath>

namespace netopt {

bool RowValidator::parseCost(std::string_view str, Cost& outCost) {
    if (str.empty()) return false;
    const char* first = str.data();
    const char* last = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(first, last, outCost);
    return ec == std::errc{} && ptr == last;
}

bool RowValidator::parseExternalId(std::string_view str, int64_t& outId) {
    if (str.empty()) return false;
    const char* first = str.data();
    const char* last = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(first, last, outId);
    return ec == std::errc{} && ptr == last;
}

bool RowValidator::parseDouble(std::string_view str, double& outVal) {
    if (str.empty()) {
        outVal = 0.0;
        return true;
    }
    const char* first = str.data();
    const char* last = str.data() + str.size();
    auto [ptr, ec] = std::from_chars(first, last, outVal);
    if (ec == std::errc{} && ptr == last) {
        return !std::isnan(outVal) && !std::isinf(outVal);
    }
    // Fallback if from_chars for double is missing in early compilers
    char* endPtr = nullptr;
    std::string s(str);
    outVal = std::strtod(s.c_str(), &endPtr);
    return endPtr == s.c_str() + s.size() && !std::isnan(outVal) && !std::isinf(outVal);
}

Result<void> RowValidator::validateAndLoadVertices(const std::vector<CsvRow>& rows,
                                                   GraphBuilder& builder,
                                                   const std::string& filename,
                                                   DiagnosticsCollector& diagnostics) {
    size_t headerRowIndex = 0;
    bool foundHeader = false;

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        if (r.isComment) {
            continue;
        }
        // First non-comment row must be header
        if (!foundHeader) {
            foundHeader = true;
            headerRowIndex = i;
            if (r.fields.size() != 5 ||
                r.fields[0] != "id" ||
                r.fields[1] != "label" ||
                r.fields[2] != "type" ||
                r.fields[3] != "x" ||
                r.fields[4] != "y") {
                diagnostics.addError(filename, r.lineNumber,
                    "Invalid vertices header. Expected: id,label,type,x,y");
                return Result<void>::err("Invalid vertices header");
            }
            break;
        }
    }

    if (!foundHeader) {
        diagnostics.addError(filename, 0, "No vertices header found; file is empty or only comments");
        return Result<void>::err("No vertices header found");
    }

    size_t dataCount = 0;
    for (size_t i = headerRowIndex + 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        if (r.isComment) {
            continue;
        }

        if (r.fields.size() != 5) {
            diagnostics.addError(filename, r.lineNumber,
                "Wrong number of columns (expected 5, found " + std::to_string(r.fields.size()) + ")");
            continue;
        }

        int64_t extId = 0;
        if (!parseExternalId(r.fields[0], extId)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric or out-of-range vertex id: '" + r.fields[0] + "'");
            continue;
        }

        double x = 0.0;
        if (!r.fields[3].empty() && !parseDouble(r.fields[3], x)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric coordinate x: '" + r.fields[3] + "'");
            continue;
        }

        double y = 0.0;
        if (!r.fields[4].empty() && !parseDouble(r.fields[4], y)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric coordinate y: '" + r.fields[4] + "'");
            continue;
        }

        auto res = builder.addVertex(extId, r.fields[1], r.fields[2], x, y, r.lineNumber);
        if (res.isErr()) {
            diagnostics.addError(filename, r.lineNumber, res.error());
            continue;
        }

        ++dataCount;
    }

    if (dataCount == 0) {
        diagnostics.addError(filename, rows[headerRowIndex].lineNumber, "No vertices defined (header only)");
        return Result<void>::err("No vertices defined");
    }

    return Result<void>::ok();
}

Result<void> RowValidator::validateAndLoadEdges(const std::vector<CsvRow>& rows,
                                                GraphBuilder& builder,
                                                const std::string& filename,
                                                DiagnosticsCollector& diagnostics) {
    size_t headerRowIndex = 0;
    bool foundHeader = false;

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        if (r.isComment) {
            continue;
        }
        if (!foundHeader) {
            foundHeader = true;
            headerRowIndex = i;
            if (r.fields.size() != 5 ||
                r.fields[0] != "source" ||
                r.fields[1] != "target" ||
                r.fields[2] != "cost" ||
                r.fields[3] != "length_m" ||
                r.fields[4] != "medium") {
                diagnostics.addError(filename, r.lineNumber,
                    "Invalid edges header. Expected: source,target,cost,length_m,medium");
                return Result<void>::err("Invalid edges header");
            }
            break;
        }
    }

    if (!foundHeader) {
        diagnostics.addError(filename, 0, "No edges header found; file is empty or only comments");
        return Result<void>::err("No edges header found");
    }

    for (size_t i = headerRowIndex + 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        if (r.isComment) {
            continue;
        }

        if (r.fields.size() != 5) {
            diagnostics.addError(filename, r.lineNumber,
                "Wrong number of columns (expected 5, found " + std::to_string(r.fields.size()) + ")");
            continue;
        }

        int64_t srcId = 0;
        if (!parseExternalId(r.fields[0], srcId)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric or out-of-range source vertex id: '" + r.fields[0] + "'");
            continue;
        }

        int64_t dstId = 0;
        if (!parseExternalId(r.fields[1], dstId)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric or out-of-range target vertex id: '" + r.fields[1] + "'");
            continue;
        }

        Cost cost = 0;
        if (!parseCost(r.fields[2], cost)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric, NaN, or out-of-range edge cost: '" + r.fields[2] + "'");
            continue;
        }

        double len = 0.0;
        if (!r.fields[3].empty() && !parseDouble(r.fields[3], len)) {
            diagnostics.addError(filename, r.lineNumber,
                "Non-numeric edge length_m: '" + r.fields[3] + "'");
            continue;
        }

        builder.addEdge(srcId, dstId, cost, len, r.fields[4], r.lineNumber);
    }

    return Result<void>::ok();
}

} // namespace netopt
