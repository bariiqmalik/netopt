#include "doctest.h"
#include "algo/mst_algorithm.hpp"
#include "core/graph_builder.hpp"

using namespace netopt;

TEST_CASE("Prim (Heap) and Prim (Dense) textbook fixture") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    // CLRS 9-vertex graph
    for (int64_t i = 0; i < 9; ++i) {
        builder.addVertex(i, "V" + std::to_string(i));
    }

    builder.addEdge(0, 1, 4);
    builder.addEdge(0, 7, 8);
    builder.addEdge(1, 2, 8);
    builder.addEdge(1, 7, 11);
    builder.addEdge(7, 8, 7);
    builder.addEdge(7, 6, 1);
    builder.addEdge(2, 8, 2);
    builder.addEdge(8, 6, 6);
    builder.addEdge(2, 3, 7);
    builder.addEdge(2, 5, 4);
    builder.addEdge(6, 5, 2);
    builder.addEdge(3, 5, 14);
    builder.addEdge(3, 4, 9);
    builder.addEdge(5, 4, 10);

    auto g = builder.build(diag).value();

    auto primHeap = AlgorithmFactory::create("Prim (Heap)");
    auto primDense = AlgorithmFactory::create("Prim (Dense)");
    auto kruskal = AlgorithmFactory::create("Kruskal");
    NullSink sink;

    auto rHeap = primHeap->run(g, sink);
    auto rDense = primDense->run(g, sink);
    auto rKruskal = kruskal->run(g, sink);

    CHECK(rHeap.connected());
    CHECK(rHeap.totalCost == 37);
    CHECK(rDense.totalCost == 37);
    CHECK(rKruskal.totalCost == 37);

    // Edge sets must be strictly identical!
    CHECK(rHeap.edges == rKruskal.edges);
    CHECK(rDense.edges == rKruskal.edges);
}

TEST_CASE("Prim on disconnected graph produces spanning forest matching Kruskal") {
    GraphBuilder builder;
    DiagnosticsCollector diag;

    builder.addVertex(0, "A");
    builder.addVertex(1, "B");
    builder.addEdge(0, 1, 15);

    builder.addVertex(2, "C");
    builder.addVertex(3, "D");
    builder.addEdge(2, 3, 25);

    builder.addVertex(4, "Solo");

    auto g = builder.build(diag).value();

    auto primHeap = AlgorithmFactory::create("Prim (Heap)");
    auto primDense = AlgorithmFactory::create("Prim (Dense)");
    auto kruskal = AlgorithmFactory::create("Kruskal");
    NullSink sink;

    auto rHeap = primHeap->run(g, sink);
    auto rDense = primDense->run(g, sink);
    auto rKruskal = kruskal->run(g, sink);

    CHECK_FALSE(rHeap.connected());
    CHECK(rHeap.components == 3);
    CHECK(rHeap.totalCost == 40);
    CHECK(rDense.totalCost == 40);
    CHECK(rKruskal.totalCost == 40);

    CHECK(rHeap.edges == rKruskal.edges);
    CHECK(rDense.edges == rKruskal.edges);
}
