#include "analysis/sensitivity.hpp"
#include "core/dsu.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>

namespace netopt {

void SensitivityAnalyzer::BinaryLifting::build(const Graph& graph, const MstResult& mst) {
    const size_t numVertices = graph.vertexCount();
    if (numVertices == 0) return;

    logN = std::max(2, static_cast<int>(std::ceil(std::log2(numVertices + 1))) + 2);
    depth.assign(numVertices, 0);
    component.assign(numVertices, -1);
    up.assign(logN, std::vector<int>(numVertices, -1));
    maxEdgeOnJump.assign(logN, std::vector<Edge>(numVertices, Edge{.u = 0, .v = 0, .cost = -1, .id = 0}));

    // Build tree adjacency
    std::vector<std::vector<std::pair<VertexId, Edge>>> treeAdj(numVertices);
    for (const auto edgeId : mst.edges) {
        const auto& e = graph.edge(edgeId);
        treeAdj[e.u].push_back({e.v, e});
        treeAdj[e.v].push_back({e.u, e});
    }

    int compId = 0;
    std::vector<VertexId> stack;

    for (VertexId root = 0; root < static_cast<VertexId>(numVertices); ++root) {
        if (component[root] != -1) continue;

        component[root] = compId;
        depth[root] = 0;
        up[0][root] = -1;
        stack.push_back(root);

        while (!stack.empty()) {
            const VertexId curr = stack.back();
            stack.pop_back();

            for (const auto& [nbr, edge] : treeAdj[curr]) {
                if (nbr == static_cast<VertexId>(up[0][curr])) continue;

                component[nbr] = compId;
                depth[nbr] = depth[curr] + 1;
                up[0][nbr] = static_cast<int>(curr);
                maxEdgeOnJump[0][nbr] = edge;
                stack.push_back(nbr);
            }
        }
        ++compId;
    }

    // Binary lifting DP table
    for (int k = 1; k < logN; ++k) {
        for (VertexId v = 0; v < static_cast<VertexId>(numVertices); ++v) {
            const int p = up[k - 1][v];
            if (p != -1) {
                up[k][v] = up[k - 1][p];
                const Edge& e1 = maxEdgeOnJump[k - 1][v];
                const Edge& e2 = maxEdgeOnJump[k - 1][p];
                maxEdgeOnJump[k][v] = (e1 < e2) ? e2 : e1;
            } else {
                up[k][v] = -1;
            }
        }
    }
}

Edge SensitivityAnalyzer::BinaryLifting::queryPathMax(VertexId u, VertexId v) const {
    Edge maxE{.u = 0, .v = 0, .cost = -1, .id = 0};
    if (component[u] != component[v]) {
        return maxE;
    }

    if (depth[u] < depth[v]) {
        std::swap(u, v);
    }

    // Lift u to depth of v
    for (int k = logN - 1; k >= 0; --k) {
        if (depth[u] - (1 << k) >= depth[v]) {
            if (maxE < maxEdgeOnJump[k][u]) {
                maxE = maxEdgeOnJump[k][u];
            }
            u = static_cast<VertexId>(up[k][u]);
        }
    }

    if (u == v) {
        return maxE;
    }

    // Lift both until LCA
    for (int k = logN - 1; k >= 0; --k) {
        if (up[k][u] != up[k][v]) {
            if (maxE < maxEdgeOnJump[k][u]) maxE = maxEdgeOnJump[k][u];
            if (maxE < maxEdgeOnJump[k][v]) maxE = maxEdgeOnJump[k][v];
            u = static_cast<VertexId>(up[k][u]);
            v = static_cast<VertexId>(up[k][v]);
        }
    }

    if (maxE < maxEdgeOnJump[0][u]) maxE = maxEdgeOnJump[0][u];
    if (maxE < maxEdgeOnJump[0][v]) maxE = maxEdgeOnJump[0][v];

    return maxE;
}

SensitivityReport SensitivityAnalyzer::analyze(const Graph& graph, const MstResult& mst) {
    SensitivityReport report;
    const size_t numVertices = graph.vertexCount();
    if (numVertices == 0) return report;

    BinaryLifting lifting;
    lifting.build(graph, mst);

    std::unordered_set<EdgeId> mstEdgeSet(mst.edges.begin(), mst.edges.end());

    // 1. Identify non-tree edges and compute their tolerance
    std::vector<Edge> nonTreeEdges;
    Cost minSecondBestDelta = std::numeric_limits<Cost>::max();
    Edge bestInsertedEdge;
    Edge bestRemovedEdge;
    bool foundSecondBest = false;

    for (const auto& e : graph.edges()) {
        if (mstEdgeSet.contains(e.id)) continue;
        nonTreeEdges.push_back(e);

        if (lifting.component[e.u] == lifting.component[e.v]) {
            const Edge maxPathE = lifting.queryPathMax(e.u, e.v);
            const Cost tolerance = e.cost - maxPathE.cost;

            report.nonTreeTolerances.push_back(NonTreeEdgeTolerance{
                .nonTreeEdgeId = e.id,
                .u = e.u,
                .v = e.v,
                .cost = e.cost,
                .maxPathEdgeId = maxPathE.id,
                .maxPathCost = maxPathE.cost,
                .tolerance = tolerance
            });

            // Evaluate second-best candidate
            if (e.cost >= maxPathE.cost) {
                const Cost delta = e.cost - maxPathE.cost;
                if (!foundSecondBest || delta < minSecondBestDelta) {
                    minSecondBestDelta = delta;
                    bestInsertedEdge = e;
                    bestRemovedEdge = maxPathE;
                    foundSecondBest = true;
                }
            }
        }
    }

    // 2. Compute tree edge resilience using path-stepping with sorted non-tree edges
    // Sort non-tree edges ascending by cost
    std::sort(nonTreeEdges.begin(), nonTreeEdges.end());

    // Tree edges indexed by tree edge id
    std::unordered_map<EdgeId, MstEdgeResilience> treeRes;
    for (const auto edgeId : mst.edges) {
        const auto& e = graph.edge(edgeId);
        treeRes[edgeId] = MstEdgeResilience{
            .treeEdgeId = edgeId,
            .u = e.u,
            .v = e.v,
            .cost = e.cost,
            .isBridge = true,
            .replacementEdgeId = std::nullopt,
            .replacementCost = 0,
            .costDelta = 0
        };
    }

    // Tree jump pointers for path compression
    std::vector<VertexId> jumpParent(numVertices);
    for (VertexId v = 0; v < static_cast<VertexId>(numVertices); ++v) {
        jumpParent[v] = v;
    }

    auto findRep = [&](auto& self, VertexId v) -> VertexId {
        if (jumpParent[v] == v) return v;
        return jumpParent[v] = self(self, jumpParent[v]);
    };

    // Orient tree edges: child -> edge to parent
    std::vector<EdgeId> parentEdge(numVertices, std::numeric_limits<EdgeId>::max());
    for (VertexId v = 0; v < static_cast<VertexId>(numVertices); ++v) {
        if (lifting.up[0][v] != -1) {
            parentEdge[v] = lifting.maxEdgeOnJump[0][v].id;
        }
    }

    auto getLca = [&](VertexId u, VertexId v) -> VertexId {
        if (lifting.depth[u] < lifting.depth[v]) std::swap(u, v);
        for (int k = lifting.logN - 1; k >= 0; --k) {
            if (lifting.depth[u] - (1 << k) >= lifting.depth[v]) {
                u = static_cast<VertexId>(lifting.up[k][u]);
            }
        }
        if (u == v) return u;
        for (int k = lifting.logN - 1; k >= 0; --k) {
            if (lifting.up[k][u] != lifting.up[k][v]) {
                u = static_cast<VertexId>(lifting.up[k][u]);
                v = static_cast<VertexId>(lifting.up[k][v]);
            }
        }
        return static_cast<VertexId>(lifting.up[0][u]);
    };

    for (const auto& nonTreeE : nonTreeEdges) {
        if (lifting.component[nonTreeE.u] != lifting.component[nonTreeE.v]) continue;

        const VertexId lca = getLca(nonTreeE.u, nonTreeE.v);

        auto stepUp = [&](VertexId curr) {
            curr = findRep(findRep, curr);
            while (lifting.depth[curr] > lifting.depth[lca]) {
                const EdgeId peId = parentEdge[curr];
                if (peId != std::numeric_limits<EdgeId>::max()) {
                    auto& entry = treeRes[peId];
                    if (entry.isBridge) {
                        entry.isBridge = false;
                        entry.replacementEdgeId = nonTreeE.id;
                        entry.replacementCost = nonTreeE.cost;
                        entry.costDelta = nonTreeE.cost - entry.cost;
                    }
                }
                const VertexId parent = static_cast<VertexId>(lifting.up[0][curr]);
                jumpParent[curr] = findRep(findRep, parent);
                curr = jumpParent[curr];
            }
        };

        stepUp(nonTreeE.u);
        stepUp(nonTreeE.v);
    }

    for (const auto edgeId : mst.edges) {
        const auto& item = treeRes[edgeId];
        report.treeEdgeResilience.push_back(item);
        if (item.isBridge) {
            report.criticalBridgeEdges.insert(edgeId);
        }
    }

    // 3. Assemble second best MST if found
    if (foundSecondBest) {
        report.secondBestMst.exists = true;
        report.secondBestMst.costIncrease = minSecondBestDelta;
        report.secondBestMst.totalCost = mst.totalCost + minSecondBestDelta;
        report.secondBestMst.insertedEdge = bestInsertedEdge.id;
        report.secondBestMst.removedEdge = bestRemovedEdge.id;

        for (const auto edgeId : mst.edges) {
            if (edgeId != bestRemovedEdge.id) {
                report.secondBestMst.edges.push_back(edgeId);
            }
        }
        report.secondBestMst.edges.push_back(bestInsertedEdge.id);
        std::sort(report.secondBestMst.edges.begin(), report.secondBestMst.edges.end());
    }

    return report;
}

std::string SensitivityReport::formatConsoleTable(const Graph& graph) const {
    std::ostringstream ss;
    ss << "\n============================== FAILURE RESILIENCE / SENSITIVITY REPORT ==============================\n";
    ss << "1. MST TREE EDGE RESILIENCE & BRIDGES (Single Points of Failure):\n";
    ss << "-----------------------------------------------------------------------------------------------------\n";
    ss << std::left << std::setw(8) << "EdgeId"
       << std::setw(24) << "Endpoints"
       << std::setw(10) << "Cost"
       << std::setw(12) << "Status"
       << std::setw(16) << "Replacement"
       << std::setw(12) << "Repl. Cost"
       << std::setw(12) << "Delta Cost" << "\n";
    ss << "-----------------------------------------------------------------------------------------------------\n";

    for (const auto& item : treeEdgeResilience) {
        const auto& u = graph.vertex(item.u);
        const auto& v = graph.vertex(item.v);
        std::string endpoints = u.label + " -- " + v.label;

        ss << std::left << std::setw(8) << item.treeEdgeId
           << std::setw(24) << endpoints
           << std::setw(10) << item.cost;

        if (item.isBridge) {
            ss << std::setw(12) << "CRITICAL"
               << std::setw(16) << "NONE (Bridge)"
               << std::setw(12) << "-"
               << std::setw(12) << "-" << "\n";
        } else {
            const auto& repU = graph.vertex(graph.edge(*item.replacementEdgeId).u);
            const auto& repV = graph.vertex(graph.edge(*item.replacementEdgeId).v);
            std::string repLabel = repU.label + "-" + repV.label;

            ss << std::setw(12) << "Protected"
               << std::setw(16) << (std::to_string(*item.replacementEdgeId) + " (" + repLabel + ")")
               << std::setw(12) << item.replacementCost
               << std::setw(12) << ("+" + std::to_string(item.costDelta)) << "\n";
        }
    }

    ss << "\n2. NON-MST EDGE TOLERANCES (Cost drop required to enter MST):\n";
    ss << "-----------------------------------------------------------------------------------------------------\n";
    ss << std::left << std::setw(8) << "EdgeId"
       << std::setw(24) << "Endpoints"
       << std::setw(12) << "Current Cost"
       << std::setw(16) << "Max Tree Edge"
       << std::setw(14) << "Max Path Cost"
       << std::setw(14) << "Cost Tolerance" << "\n";
    ss << "-----------------------------------------------------------------------------------------------------\n";

    for (const auto& item : nonTreeTolerances) {
        const auto& u = graph.vertex(item.u);
        const auto& v = graph.vertex(item.v);
        std::string endpoints = u.label + " -- " + v.label;

        ss << std::left << std::setw(8) << item.nonTreeEdgeId
           << std::setw(24) << endpoints
           << std::setw(12) << item.cost
           << std::setw(16) << ("Edge #" + std::to_string(item.maxPathEdgeId))
           << std::setw(14) << item.maxPathCost
           << std::setw(14) << item.tolerance << "\n";
    }

    ss << "\n3. SECOND-BEST MINIMUM SPANNING TREE:\n";
    ss << "-----------------------------------------------------------------------------------------------------\n";
    if (secondBestMst.exists) {
        ss << "Status: Found alternative spanning tree\n";
        ss << "Total Cost: " << secondBestMst.totalCost << " (Delta: +" << secondBestMst.costIncrease << ")\n";
        ss << "Substituted: Removed Tree Edge #" << secondBestMst.removedEdge
           << ", Inserted Alternate Edge #" << secondBestMst.insertedEdge << "\n";
    } else {
        ss << "Status: No alternative spanning tree exists (network is a tree or forest with no cycles)\n";
    }
    ss << "=====================================================================================================\n";
    return ss.str();
}

std::string SensitivityReport::toCsv(const Graph& graph) const {
    std::ostringstream ss;
    ss << "#netopt-schema:1\n";
    ss << "section,edge_id,source,target,cost,status,replacement_id,replacement_cost,cost_delta\n";

    for (const auto& item : treeEdgeResilience) {
        const auto& u = graph.vertex(item.u);
        const auto& v = graph.vertex(item.v);
        ss << "tree_resilience,"
           << item.treeEdgeId << ","
           << u.externalId << ","
           << v.externalId << ","
           << item.cost << ","
           << (item.isBridge ? "BRIDGE" : "PROTECTED") << ",";
        if (item.replacementEdgeId) {
            ss << *item.replacementEdgeId << ","
               << item.replacementCost << ","
               << item.costDelta << "\n";
        } else {
            ss << ",,\n";
        }
    }

    for (const auto& item : nonTreeTolerances) {
        const auto& u = graph.vertex(item.u);
        const auto& v = graph.vertex(item.v);
        ss << "non_tree_tolerance,"
           << item.nonTreeEdgeId << ","
           << u.externalId << ","
           << v.externalId << ","
           << item.cost << ","
           << "TOLERANCE,"
           << item.maxPathEdgeId << ","
           << item.maxPathCost << ","
           << item.tolerance << "\n";
    }

    return ss.str();
}

} // namespace netopt
