#include "analysis/verifier.hpp"
#include "core/dsu.hpp"
#include <unordered_set>
#include <vector>

namespace netopt {

bool Verifier::findPathMax(VertexId curr,
                           VertexId target,
                           VertexId parent,
                           const std::vector<std::vector<std::pair<VertexId, Edge>>>& treeAdj,
                           Edge& outMaxEdge) {
    if (curr == target) {
        return true;
    }

    for (const auto& [nbr, edge] : treeAdj[curr]) {
        if (nbr == parent) {
            continue;
        }
        Edge subMax = edge;
        if (findPathMax(nbr, target, curr, treeAdj, subMax)) {
            if (edge < subMax) {
                outMaxEdge = subMax;
            } else {
                outMaxEdge = edge;
            }
            return true;
        }
    }

    return false;
}

VerificationReport Verifier::verify(const Graph& graph, const MstResult& result) {
    const size_t numVertices = graph.vertexCount();
    if (numVertices == 0) {
        return VerificationReport{
            .valid = true,
            .expectedEdges = 0,
            .actualEdges = 0,
            .components = 0,
            .failureReason = ""
        };
    }

    // 1. Determine actual connected components of the original graph
    DisjointSet origDsu(numVertices);
    for (const auto& e : graph.edges()) {
        origDsu.unite(e.u, e.v);
    }
    const size_t expectedComponents = origDsu.components();
    const size_t expectedEdges = numVertices - expectedComponents;

    VerificationReport report{
        .valid = true,
        .expectedEdges = expectedEdges,
        .actualEdges = result.edges.size(),
        .components = expectedComponents,
        .failureReason = ""
    };

    // 2. Check edge count
    if (result.edges.size() != expectedEdges) {
        report.valid = false;
        report.failureReason = "Edge count mismatch: expected " + std::to_string(expectedEdges) +
                               " (V - c = " + std::to_string(numVertices) + " - " +
                               std::to_string(expectedComponents) + "), but got " +
                               std::to_string(result.edges.size());
        return report;
    }

    // 3. Check acyclicity and spanning
    DisjointSet treeDsu(numVertices);
    std::unordered_set<EdgeId> treeEdgeIds;
    std::vector<std::vector<std::pair<VertexId, Edge>>> treeAdj(numVertices);

    for (const auto edgeId : result.edges) {
        if (edgeId >= graph.edgeCount()) {
            report.valid = false;
            report.failureReason = "Invalid edge id in result: " + std::to_string(edgeId);
            return report;
        }

        const auto& e = graph.edge(edgeId);
        treeEdgeIds.insert(edgeId);
        treeAdj[e.u].push_back({e.v, e});
        treeAdj[e.v].push_back({e.u, e});

        if (!treeDsu.unite(e.u, e.v)) {
            report.valid = false;
            report.failureReason = "Cycle detected in tree: edge " + std::to_string(edgeId) +
                                   " connects vertices (" + std::to_string(e.u) + ", " +
                                   std::to_string(e.v) + ") already connected";
            return report;
        }
    }

    if (treeDsu.components() != expectedComponents) {
        report.valid = false;
        report.failureReason = "Tree components (" + std::to_string(treeDsu.components()) +
                               ") does not match original graph components (" +
                               std::to_string(expectedComponents) + ")";
        return report;
    }

    // 4. Check cycle property (optimality)
    for (const auto& nonTreeEdge : graph.edges()) {
        if (treeEdgeIds.contains(nonTreeEdge.id)) {
            continue;
        }

        // Only test if endpoints are in same component
        if (origDsu.connected(nonTreeEdge.u, nonTreeEdge.v)) {
            Edge maxPathEdge{.u = 0, .v = 0, .cost = 0, .id = 0};
            if (findPathMax(nonTreeEdge.u, nonTreeEdge.v, numVertices, treeAdj, maxPathEdge)) {
                // Non-tree edge must NOT be cheaper than the maximum edge on its tree path
                if (nonTreeEdge < maxPathEdge) {
                    report.valid = false;
                    report.failureReason = "Cycle property violation: non-tree edge " +
                                           std::to_string(nonTreeEdge.id) + " (cost " +
                                           std::to_string(nonTreeEdge.cost) +
                                           ") is cheaper than tree path edge " +
                                           std::to_string(maxPathEdge.id) + " (cost " +
                                           std::to_string(maxPathEdge.cost) + ")";
                    return report;
                }
            }
        }
    }

    return report;
}

} // namespace netopt
