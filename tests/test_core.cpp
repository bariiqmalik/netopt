#include "doctest.h"
#include "core/types.hpp"
#include "core/graph.hpp"
#include "core/graph_builder.hpp"
#include "io/diagnostics.hpp"

using namespace netopt;

TEST_CASE("Result<T> operations and value semantics") {
    SUBCASE("Result<int> Ok value") {
        auto res = Result<int>::ok(42);
        CHECK(res.isOk());
        CHECK_FALSE(res.isErr());
        CHECK(static_cast<bool>(res));
        CHECK(res.value() == 42);
    }

    SUBCASE("Result<int> Err value") {
        auto res = Result<int>::err("Calculation failed");
        CHECK_FALSE(res.isOk());
        CHECK(res.isErr());
        CHECK_FALSE(static_cast<bool>(res));
        CHECK(res.error() == "Calculation failed");
        CHECK_THROWS_AS((void)res.value(), std::runtime_error);
    }

    SUBCASE("Result<void> operations") {
        auto okRes = Result<void>::ok();
        CHECK(okRes.isOk());
        CHECK_FALSE(okRes.isErr());

        auto errRes = Result<void>::err("Operation failed");
        CHECK_FALSE(errRes.isOk());
        CHECK(errRes.isErr());
        CHECK(errRes.error() == "Operation failed");
    }
}

TEST_CASE("Edge total ordering by (cost, id)") {
    Edge e1{.u = 0, .v = 1, .cost = 100, .id = 1};
    Edge e2{.u = 1, .v = 2, .cost = 100, .id = 2};
    Edge e3{.u = 2, .v = 3, .cost = 50,  .id = 5};

    // Cost primary
    CHECK(e3 < e1);
    CHECK(e3 < e2);

    // Id tie-breaker when costs equal
    CHECK(e1 < e2);
    CHECK_FALSE(e2 < e1);
    CHECK(e1 == e1);
}

TEST_CASE("GraphBuilder CSR construction and parallel edge deduplication") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    CHECK(builder.addVertex(10, "A").isOk());
    CHECK(builder.addVertex(20, "B").isOk());
    CHECK(builder.addVertex(30, "C").isOk());

    // Duplicate vertex ID
    auto dupRes = builder.addVertex(10, "Duplicate");
    CHECK(dupRes.isErr());

    // Add edges:
    // (10, 20) with cost 1500
    // Parallel (10, 20) with cost 1200 (cheaper, should be kept)
    // Parallel (20, 10) with cost 2000 (more expensive, should be dropped)
    // Self-loop (30, 30) with cost 100 (should be dropped)
    // Edge (20, 30) with cost 800
    CHECK(builder.addEdge(10, 20, 1500, 10.0, "cat6", 1).isOk());
    CHECK(builder.addEdge(10, 20, 1200, 10.0, "cat6", 2).isOk());
    CHECK(builder.addEdge(20, 10, 2000, 10.0, "cat6", 3).isOk());
    CHECK(builder.addEdge(30, 30, 100, 1.0, "fiber", 4).isOk());
    CHECK(builder.addEdge(20, 30, 800, 5.0, "fiber", 5).isOk());

    auto gRes = builder.build(diag);
    CHECK(gRes.isOk());
    CHECK(diag.hasWarnings());
    CHECK_FALSE(diag.hasErrors());

    const auto& g = gRes.value();
    CHECK(g.vertexCount() == 3);
    CHECK(g.edgeCount() == 2); // only two valid distinct edges kept

    // Verify kept edge (10, 20) has cost 1200
    const auto& e0 = g.edge(0);
    CHECK(e0.cost == 1200);

    const auto& e1 = g.edge(1);
    CHECK(e1.cost == 800);

    // Verify CSR neighbor spans
    const VertexId denseB = g.findDenseId(20).value();
    auto neighborsB = g.neighbors(denseB);
    CHECK(neighborsB.size() == 2);
}
