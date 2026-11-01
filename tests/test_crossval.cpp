#include "doctest.h"
#include "algo/mst_algorithm.hpp"
#include "core/graph_builder.hpp"
#include <random>

using namespace netopt;

TEST_CASE("Cross-validation: 1000+ random graphs (sparse, dense, disconnected, ties-heavy, single-vertex)") {
    std::mt19937_64 rng(42133742);
    auto kruskal = AlgorithmFactory::create("Kruskal");
    auto primHeap = AlgorithmFactory::create("Prim (Heap)");
    auto primDense = AlgorithmFactory::create("Prim (Dense)");
    NullSink sink;

    constexpr int kTotalRuns = 1050; // Meets and exceeds 1000 requirement

    for (int iter = 0; iter < kTotalRuns; ++iter) {
        GraphBuilder builder;
        DiagnosticsCollector diag;

        // Choose scenario based on iteration
        const int scenario = iter % 6;
        size_t vCount = 1;

        if (scenario == 0) {
            // Single vertex or tiny
            vCount = (iter % 3) + 1;
        } else if (scenario == 1) {
            // Sparse graph
            std::uniform_int_distribution<size_t> vDist(3, 35);
            vCount = vDist(rng);
        } else if (scenario == 2) {
            // Dense graph
            std::uniform_int_distribution<size_t> vDist(3, 25);
            vCount = vDist(rng);
        } else if (scenario == 3) {
            // Disconnected graph (components)
            std::uniform_int_distribution<size_t> vDist(4, 30);
            vCount = vDist(rng);
        } else if (scenario == 4) {
            // Ties-heavy graph
            std::uniform_int_distribution<size_t> vDist(3, 25);
            vCount = vDist(rng);
        } else {
            // Complete or near-complete graph
            std::uniform_int_distribution<size_t> vDist(2, 20);
            vCount = vDist(rng);
        }

        for (size_t i = 0; i < vCount; ++i) {
            builder.addVertex(static_cast<int64_t>(i), "V" + std::to_string(i));
        }

        if (vCount > 1) {
            std::uniform_int_distribution<Cost> tieCostDist(1, 4); // for ties
            std::uniform_int_distribution<Cost> wideCostDist(1, 10000);
            std::uniform_real_distribution<double> edgeProbDist(0.0, 1.0);

            double pEdge = 0.3;
            if (scenario == 1) pEdge = 0.15;
            else if (scenario == 2) pEdge = 0.7;
            else if (scenario == 3) pEdge = 0.1; // disconnected
            else if (scenario == 5) pEdge = 0.95; // near complete

            for (size_t i = 0; i < vCount; ++i) {
                for (size_t j = i + 1; j < vCount; ++j) {
                    if (edgeProbDist(rng) < pEdge) {
                        Cost c = (scenario == 4) ? tieCostDist(rng) : wideCostDist(rng);
                        builder.addEdge(static_cast<int64_t>(i), static_cast<int64_t>(j), c);
                    }
                }
            }
        }

        auto gRes = builder.build(diag);
        REQUIRE(gRes.isOk());
        const auto& g = gRes.value();

        auto rK = kruskal->run(g, sink);
        auto rH = primHeap->run(g, sink);
        auto rD = primDense->run(g, sink);

        // All three must produce identical total cost and components
        CHECK(rH.totalCost == rK.totalCost);
        CHECK(rD.totalCost == rK.totalCost);
        CHECK(rH.components == rK.components);
        CHECK(rD.components == rK.components);

        // Strict total ordering guarantees 100% IDENTICAL edge sets
        CHECK(rH.edges == rK.edges);
        CHECK(rD.edges == rK.edges);
    }
}
