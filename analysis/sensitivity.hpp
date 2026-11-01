#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>

namespace netopt {

struct MstEdgeResilience {
    EdgeId treeEdgeId;
    VertexId u;
    VertexId v;
    Cost cost;
    bool isBridge{false}; // Single Point of Failure (SPOF)
    std::optional<EdgeId> replacementEdgeId;
    Cost replacementCost{0};
    Cost costDelta{0};
};

struct NonTreeEdgeTolerance {
    EdgeId nonTreeEdgeId;
    VertexId u;
    VertexId v;
    Cost cost;
    EdgeId maxPathEdgeId;
    Cost maxPathCost;
    Cost tolerance; // How much cost must decrease to enter the MST
};

struct SecondBestMstResult {
    bool exists{false};
    Cost totalCost{0};
    Cost costIncrease{0};
    std::vector<EdgeId> edges;
    EdgeId insertedEdge;
    EdgeId removedEdge;
};

struct SensitivityReport {
    std::vector<MstEdgeResilience> treeEdgeResilience;
    std::vector<NonTreeEdgeTolerance> nonTreeTolerances;
    SecondBestMstResult secondBestMst;
    std::unordered_set<EdgeId> criticalBridgeEdges;

    [[nodiscard]] std::string formatConsoleTable(const Graph& graph) const;
    [[nodiscard]] std::string toCsv(const Graph& graph) const;
};

class SensitivityAnalyzer {
public:
    static SensitivityReport analyze(const Graph& graph, const MstResult& mst);

private:
    struct BinaryLifting {
        int logN{0};
        std::vector<int> depth;
        std::vector<int> component;
        std::vector<std::vector<int>> up;
        std::vector<std::vector<Edge>> maxEdgeOnJump;

        void build(const Graph& graph, const MstResult& mst);
        [[nodiscard]] Edge queryPathMax(VertexId u, VertexId v) const;
    };
};

} // namespace netopt
