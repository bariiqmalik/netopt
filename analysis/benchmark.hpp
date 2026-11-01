#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include "algo/mst_algorithm.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace netopt {

struct BenchmarkMetric {
    std::string algorithm;
    std::string graphType; // "sparse" or "dense"
    size_t vertices{0};
    size_t edges{0};
    double density{0.0};
    double durationMs{0.0};
    uint64_t operations{0};
    Cost mstCost{0};
};

class BenchmarkTraceSink : public ITraceSink {
public:
    void onEdgeConsidered(EdgeId, VertexId, VertexId, Cost) override { ++operations_; }
    void onEdgeAccepted(EdgeId, VertexId, VertexId, Cost) override { ++operations_; }
    void onEdgeRejected(EdgeId, VertexId, VertexId, Cost, const std::string&) override { ++operations_; }

    [[nodiscard]] uint64_t operations() const noexcept { return operations_; }
    void reset() noexcept { operations_ = 0; }

private:
    uint64_t operations_{0};
};

class BenchmarkSuite {
public:
    static std::vector<BenchmarkMetric> run(const std::vector<size_t>& vertexCounts,
                                           bool includeDense = true,
                                           size_t trials = 3);

    static Result<void> saveToCsv(const std::vector<BenchmarkMetric>& metrics,
                                 const std::string& filepath);

    static Graph generateRandomGraph(size_t vertices, double density, uint64_t seed);
};

} // namespace netopt
