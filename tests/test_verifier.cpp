#include "doctest.h"
#include "analysis/verifier.hpp"
#include "algo/mst_algorithm.hpp"
#include "core/graph_builder.hpp"

using namespace netopt;

TEST_CASE("Verifier self-checking suite") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    // Triangle: 0-1 (10), 1-2 (20), 0-2 (30)
    builder.addVertex(0, "A");
    builder.addVertex(1, "B");
    builder.addVertex(2, "C");
    builder.addEdge(0, 1, 10); // edge 0
    builder.addEdge(1, 2, 20); // edge 1
    builder.addEdge(0, 2, 30); // edge 2

    auto g = builder.build(diag).value();

    SUBCASE("Valid optimal MST passes") {
        MstResult validMst{
            .edges = {0, 1},
            .totalCost = 30,
            .components = 1
        };
        auto rep = Verifier::verify(g, validMst);
        CHECK(rep.valid);
        CHECK(rep.actualEdges == 2);
    }

    SUBCASE("Cycle in edge set fails") {
        MstResult cycleMst{
            .edges = {0, 1, 2},
            .totalCost = 60,
            .components = 1
        };
        auto rep = Verifier::verify(g, cycleMst);
        CHECK_FALSE(rep.valid);
        CHECK(rep.failureReason.find("Edge count mismatch") != std::string::npos);
    }

    SUBCASE("Suboptimal edge set fails cycle property") {
        // Choose edge 0 (10) and edge 2 (30) instead of 1 (20)
        MstResult subMst{
            .edges = {0, 2},
            .totalCost = 40,
            .components = 1
        };
        auto rep = Verifier::verify(g, subMst);
        CHECK_FALSE(rep.valid);
        CHECK(rep.failureReason.find("Cycle property violation") != std::string::npos);
    }
}
