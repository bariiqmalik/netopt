#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include <string>
#include <memory>
#include <vector>

namespace netopt {

class ITraceSink {
public:
    virtual ~ITraceSink() = default;

    virtual void onEdgeConsidered(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
        (void)edgeId; (void)u; (void)v; (void)cost;
    }

    virtual void onEdgeAccepted(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
        (void)edgeId; (void)u; (void)v; (void)cost;
    }

    virtual void onEdgeRejected(EdgeId edgeId, VertexId u, VertexId v, Cost cost, const std::string& reason) {
        (void)edgeId; (void)u; (void)v; (void)cost; (void)reason;
    }

    virtual void onStep(const std::string& description) {
        (void)description;
    }
};

class NullSink : public ITraceSink {
public:
    void onEdgeConsidered(EdgeId, VertexId, VertexId, Cost) override {}
    void onEdgeAccepted(EdgeId, VertexId, VertexId, Cost) override {}
    void onEdgeRejected(EdgeId, VertexId, VertexId, Cost, const std::string&) override {}
    void onStep(const std::string&) override {}
};

class IMstAlgorithm {
public:
    virtual ~IMstAlgorithm() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    virtual MstResult run(const Graph& graph, ITraceSink& sink) = 0;
};

class AlgorithmFactory {
public:
    static std::unique_ptr<IMstAlgorithm> create(const std::string& algorithmName);
    static std::unique_ptr<IMstAlgorithm> createAuto(const Graph& graph, double densityThreshold = 0.4);
    static std::vector<std::string> availableAlgorithms();
};

} // namespace netopt
