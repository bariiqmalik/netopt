#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include <string>
#include <unordered_set>

namespace netopt {

class DotExporter {
public:
    static bool isDotAvailable();

    static Result<void> exportGraph(const Graph& graph,
                                   const std::string& dotFilePath,
                                   const std::string& title = "Network Topology");

    static Result<void> exportMst(const Graph& graph,
                                 const MstResult& mst,
                                 const std::string& dotFilePath,
                                 const std::unordered_set<EdgeId>& criticalEdges = {},
                                 const std::string& title = "Minimum Spanning Tree");

    static Result<std::string> renderToPng(const std::string& dotFilePath,
                                          const std::string& pngFilePath);
};

} // namespace netopt
