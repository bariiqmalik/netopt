#include "analysis/benchmark.hpp"
#include "core/graph_builder.hpp"
#include <random>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace netopt {

Graph BenchmarkSuite::generateRandomGraph(size_t vertices, double density, uint64_t seed) {
    GraphBuilder builder;
    DiagnosticsCollector diag;
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Cost> costDist(1, 10000);
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    for (size_t i = 0; i < vertices; ++i) {
        builder.addVertex(static_cast<int64_t>(i), "V" + std::to_string(i));
    }

    if (density < 0.05) {
        // Sparse mode: ensure connected backbone + extra random edges
        for (size_t i = 1; i < vertices; ++i) {
            std::uniform_int_distribution<size_t> pDist(0, i - 1);
            size_t parent = pDist(rng);
            builder.addEdge(static_cast<int64_t>(parent), static_cast<int64_t>(i), costDist(rng));
        }
        // Add random sparse edges: target average degree ~ 4
        size_t extraEdges = vertices * 2;
        for (size_t k = 0; k < extraEdges; ++k) {
            std::uniform_int_distribution<size_t> vDist(0, vertices - 1);
            size_t u = vDist(rng);
            size_t v = vDist(rng);
            if (u != v) {
                builder.addEdge(static_cast<int64_t>(u), static_cast<int64_t>(v), costDist(rng));
            }
        }
    } else {
        // Dense mode: Erdos-Renyi G(n, p)
        for (size_t i = 0; i < vertices; ++i) {
            for (size_t j = i + 1; j < vertices; ++j) {
                if (probDist(rng) <= density) {
                    builder.addEdge(static_cast<int64_t>(i), static_cast<int64_t>(j), costDist(rng));
                }
            }
        }
    }

    auto gRes = builder.build(diag);
    if (gRes.isErr()) {
        // Fallback: minimal single vertex
        GraphBuilder fb;
        fb.addVertex(0, "Solo");
        return fb.build(diag).value();
    }
    return gRes.value();
}

std::vector<BenchmarkMetric> BenchmarkSuite::run(const std::vector<size_t>& vertexCounts,
                                                 bool includeDense,
                                                 size_t trials) {
    std::vector<BenchmarkMetric> results;
    auto algorithms = {"Kruskal", "Prim (Heap)", "Prim (Dense)"};

    for (size_t v : vertexCounts) {
        // 1. Sparse benchmark
        {
            Graph g = generateRandomGraph(v, 0.01, 1000 + v);
            for (const char* algoName : algorithms) {
                // Skip Prim Dense on very large sparse graphs (> 5000) to keep runtimes practical
                if (std::string(algoName) == "Prim (Dense)" && v > 5000) continue;

                auto algo = AlgorithmFactory::create(algoName);
                BenchmarkTraceSink sink;

                double totalMs = 0.0;
                uint64_t ops = 0;
                Cost mstCost = 0;

                for (size_t t = 0; t < trials; ++t) {
                    sink.reset();
                    auto tStart = std::chrono::steady_clock::now();
                    auto r = algo->run(g, sink);
                    auto tEnd = std::chrono::steady_clock::now();

                    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
                    totalMs += ms;
                    ops = sink.operations();
                    mstCost = r.totalCost;
                }

                results.push_back(BenchmarkMetric{
                    .algorithm = algoName,
                    .graphType = "sparse",
                    .vertices = g.vertexCount(),
                    .edges = g.edgeCount(),
                    .density = g.density(),
                    .durationMs = totalMs / static_cast<double>(trials),
                    .operations = ops,
                    .mstCost = mstCost
                });
            }
        }

        // 2. Dense benchmark (limit to max 2000 vertices for dense to avoid massive memory/time)
        if (includeDense && v <= 2000) {
            Graph g = generateRandomGraph(v, 0.40, 2000 + v);
            for (const char* algoName : algorithms) {
                auto algo = AlgorithmFactory::create(algoName);
                BenchmarkTraceSink sink;

                double totalMs = 0.0;
                uint64_t ops = 0;
                Cost mstCost = 0;

                for (size_t t = 0; t < trials; ++t) {
                    sink.reset();
                    auto tStart = std::chrono::steady_clock::now();
                    auto r = algo->run(g, sink);
                    auto tEnd = std::chrono::steady_clock::now();

                    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
                    totalMs += ms;
                    ops = sink.operations();
                    mstCost = r.totalCost;
                }

                results.push_back(BenchmarkMetric{
                    .algorithm = algoName,
                    .graphType = "dense",
                    .vertices = g.vertexCount(),
                    .edges = g.edgeCount(),
                    .density = g.density(),
                    .durationMs = totalMs / static_cast<double>(trials),
                    .operations = ops,
                    .mstCost = mstCost
                });
            }
        }
    }

    return results;
}

Result<void> BenchmarkSuite::saveToCsv(const std::vector<BenchmarkMetric>& metrics,
                                      const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        return Result<void>::err("Failed to open file for benchmarks CSV: " + filepath);
    }

    out << "#netopt-schema:1\n";
    out << "algorithm,graph_type,vertices,edges,density,duration_ms,operations,mst_cost\n";
    for (const auto& m : metrics) {
        out << m.algorithm << ","
            << m.graphType << ","
            << m.vertices << ","
            << m.edges << ","
            << std::fixed << std::setprecision(4) << m.density << ","
            << std::setprecision(4) << m.durationMs << ","
            << m.operations << ","
            << m.mstCost << "\n";
    }

    return Result<void>::ok();
}

} // namespace netopt
