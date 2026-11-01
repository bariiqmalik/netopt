#include "doctest.h"
#include "analysis/sensitivity.hpp"
#include "algo/mst_algorithm.hpp"
#include "core/graph_builder.hpp"

using namespace netopt;

TEST_CASE("Sensitivity Analysis: Bridges, Replacements, and Second-Best MST") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    // Diamond with central edge:
    // 0 - 1 (10)
    // 1 - 2 (20)
    // 2 - 3 (10)
    // 3 - 0 (20)
    // 0 - 2 (15) [diagonal]
    // 3 - 4 (100) [pendant branch to vertex 4: MUST be a bridge!]
    builder.addVertex(0, "V0");
    builder.addVertex(1, "V1");
    builder.addVertex(2, "V2");
    builder.addVertex(3, "V3");
    builder.addVertex(4, "V4");

    builder.addEdge(0, 1, 10);  // e0
    builder.addEdge(1, 2, 20);  // e1
    builder.addEdge(2, 3, 10);  // e2
    builder.addEdge(3, 0, 20);  // e3
    builder.addEdge(0, 2, 15);  // e4
    builder.addEdge(3, 4, 100); // e5 (Bridge!)

    auto g = builder.build(diag).value();
    auto kruskal = AlgorithmFactory::create("Kruskal");
    NullSink sink;
    auto mst = kruskal->run(g, sink);

    auto rep = SensitivityAnalyzer::analyze(g, mst);

    SUBCASE("Critical bridge detection") {
        // Edge (3, 4) with cost 100 has no replacement edge
        const auto& bridgeEdge = g.edge(5);
        CHECK(bridgeEdge.cost == 100);
        CHECK(rep.criticalBridgeEdges.contains(5));

        bool foundBridgeInReport = false;
        for (const auto& item : rep.treeEdgeResilience) {
            if (item.treeEdgeId == 5) {
                foundBridgeInReport = true;
                CHECK(item.isBridge);
                CHECK_FALSE(item.replacementEdgeId.has_value());
            }
        }
        CHECK(foundBridgeInReport);
    }

    SUBCASE("Protected tree edges have replacements") {
        for (const auto& item : rep.treeEdgeResilience) {
            if (item.treeEdgeId != 5) {
                CHECK_FALSE(item.isBridge);
                CHECK(item.replacementEdgeId.has_value());
                CHECK(item.costDelta >= 0);
            }
        }
    }

    SUBCASE("Second-best MST is valid and computed") {
        CHECK(rep.secondBestMst.exists);
        CHECK(rep.secondBestMst.costIncrease > 0);
        CHECK(rep.secondBestMst.totalCost > mst.totalCost);
        CHECK(rep.secondBestMst.edges.size() == mst.edges.size());
    }
}
