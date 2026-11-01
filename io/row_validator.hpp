#pragma once

#include "core/types.hpp"
#include "core/graph_builder.hpp"
#include "io/csv_tokenizer.hpp"
#include "io/diagnostics.hpp"
#include <string>
#include <vector>

namespace netopt {

class RowValidator {
public:
    static Result<void> validateAndLoadVertices(const std::vector<CsvRow>& rows,
                                               GraphBuilder& builder,
                                               const std::string& filename,
                                               DiagnosticsCollector& diagnostics);

    static Result<void> validateAndLoadEdges(const std::vector<CsvRow>& rows,
                                            GraphBuilder& builder,
                                            const std::string& filename,
                                            DiagnosticsCollector& diagnostics);

private:
    static bool parseCost(std::string_view str, Cost& outCost);
    static bool parseExternalId(std::string_view str, int64_t& outId);
    static bool parseDouble(std::string_view str, double& outVal);
};

} // namespace netopt
