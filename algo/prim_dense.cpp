#include "algo/mst_algorithm.hpp"
#include <algorithm>
#include <limits>

namespace netopt {

class PrimDenseAlgorithm : public IMstAlgorithm {
public:
    [[nodiscard]] std::string name() const override {
        return "Prim (Dense)";
    }

    MstResult run(const Graph& graph, ITraceSink& sink) override {
        const size_t numVertices = graph.vertexCount();
        if (numVertices == 0) {
            return MstResult{.edges = {}, .totalCost = 0, .components = 0};
        }

        constexpr Cost kInfCost = std::numeric_limits<Cost>::max();
        constexpr EdgeId kNoEdge = std::numeric_limits<EdgeId>::max();

        std::vector<Cost> minDist(numVertices, kInfCost);
        std::vector<EdgeId> bestEdge(numVertices, kNoEdge);
        std::vector<bool> inTree(numVertices, false);

        std::vector<EdgeId> acceptedEdges;
        acceptedEdges.reserve(numVertices > 0 ? numVertices - 1 : 0);
        Cost totalCost = 0;
        size_t components = 0;

        for (VertexId startVertex = 0; startVertex < static_cast<VertexId>(numVertices); ++startVertex) {
            if (inTree[startVertex]) {
                continue;
            }

            ++components;
            inTree[startVertex] = true;

            for (const auto& arc : graph.neighbors(startVertex)) {
                sink.onEdgeConsidered(arc.id, startVertex, arc.target, arc.cost);
                if (arc.cost < minDist[arc.target] ||
                   (arc.cost == minDist[arc.target] && arc.id < bestEdge[arc.target])) {
                    minDist[arc.target] = arc.cost;
                    bestEdge[arc.target] = arc.id;
                }
            }

            while (true) {
                VertexId bestV = static_cast<VertexId>(numVertices);
                Cost bestKey = kInfCost;
                EdgeId bestE = kNoEdge;

                for (VertexId v = 0; v < static_cast<VertexId>(numVertices); ++v) {
                    if (!inTree[v] && minDist[v] < kInfCost) {
                        if (minDist[v] < bestKey ||
                           (minDist[v] == bestKey && bestEdge[v] < bestE)) {
                            bestKey = minDist[v];
                            bestE = bestEdge[v];
                            bestV = v;
                        }
                    }
                }

                if (bestV == static_cast<VertexId>(numVertices)) {
                    break;
                }

                inTree[bestV] = true;
                acceptedEdges.push_back(bestE);
                totalCost += bestKey;

                const auto& edgeObj = graph.edge(bestE);
                sink.onEdgeAccepted(bestE, edgeObj.u, edgeObj.v, edgeObj.cost);

                for (const auto& arc : graph.neighbors(bestV)) {
                    if (inTree[arc.target]) {
                        sink.onEdgeRejected(arc.id, bestV, arc.target, arc.cost, "Endpoints already connected");
                    } else {
                        sink.onEdgeConsidered(arc.id, bestV, arc.target, arc.cost);
                        if (arc.cost < minDist[arc.target] ||
                           (arc.cost == minDist[arc.target] && arc.id < bestEdge[arc.target])) {
                            minDist[arc.target] = arc.cost;
                            bestEdge[arc.target] = arc.id;
                        }
                    }
                }
            }
        }

        std::sort(acceptedEdges.begin(), acceptedEdges.end());

        return MstResult{
            .edges = std::move(acceptedEdges),
            .totalCost = totalCost,
            .components = components
        };
    }
};

std::unique_ptr<IMstAlgorithm> makePrimDense() {
    return std::make_unique<PrimDenseAlgorithm>();
}

} // namespace netopt
