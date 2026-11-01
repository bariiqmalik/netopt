#include "doctest.h"
#include "algo/mst_algorithm.hpp"
#include "core/graph_builder.hpp"

using namespace netopt;

TEST_CASE("Kruskal's algorithm with textbook fixture") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    // CLRS 9-vertex graph (a=0 to i=8)
    for (int64_t i = 0; i < 9; ++i) {
        builder.addVertex(i, "V" + std::to_string(i));
    }

    builder.addEdge(0, 1, 4, 0, "");
    builder.addEdge(0, 7, 8, 0, "");
    builder.addEdge(1, 2, 8, 0, "");
    builder.addEdge(1, 7, 11, 0, "");
    builder.addEdge(7, 8, 7, 0, "");
    builder.addEdge(7, 6, 1, 0, "");
    builder.addEdge(2, 8, 2, 0, "");
    builder.addEdge(8, 6, 6, 0, "");
    builder.addEdge(2, 3, 7, 0, "");
    builder.addEdge(2, 5, 4, 0, "");
    builder.addEdge(6, 5, 2, 0, "");
    builder.addEdge(3, 5, 14, 0, "");
    builder.addEdge(3, 4, 9, 0, "");
    builder.addEdge(5, 4, 10, 0, "");

    auto gRes = builder.build(diag);
    REQUIRE(gRes.isOk());
    const auto& g = gRes.value();

    auto kruskal = AlgorithmFactory::create("Kruskal");
    REQUIRE(kruskal != nullptr);

    NullSink sink;
    auto result = kruskal->run(g, sink);

    CHECK(result.connected());
    CHECK(result.components == 1);
    CHECK(result.edges.size() == 8);
    CHECK(result.totalCost == 37);
}

TEST_CASE("Kruskal's algorithm on disconnected graph produces spanning forest") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    // Component 1: 0-1 (cost 10)
    builder.addVertex(0, "A");
    builder.addVertex(1, "B");
    builder.addEdge(0, 1, 10);

    // Component 2: 2-3 (cost 20)
    builder.addVertex(2, "C");
    builder.addVertex(3, "D");
    builder.addEdge(2, 3, 20);

    // Isolated vertex: 4
    builder.addVertex(4, "Isolated");

    auto g = builder.build(diag).value();
    auto kruskal = AlgorithmFactory::create("Kruskal");
    NullSink sink;
    auto result = kruskal->run(g, sink);

    CHECK_FALSE(result.connected());
    CHECK(result.components == 3);
    CHECK(result.edges.size() == 2);
    CHECK(result.totalCost == 30);
}
