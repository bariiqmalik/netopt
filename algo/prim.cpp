#include "algo/mst_algorithm.hpp"
#include "core/indexed_heap.hpp"
#include <algorithm>

namespace netopt {

class PrimHeapAlgorithm : public IMstAlgorithm {
public:
    [[nodiscard]] std::string name() const override {
        return "Prim (Heap)";
    }

    MstResult run(const Graph& graph, ITraceSink& sink) override {
        const size_t numVertices = graph.vertexCount();
        if (numVertices == 0) {
            return MstResult{.edges = {}, .totalCost = 0, .components = 0};
        }

        std::vector<bool> inTree(numVertices, false);
        std::vector<EdgeId> acceptedEdges;
        acceptedEdges.reserve(numVertices > 0 ? numVertices - 1 : 0);
        Cost totalCost = 0;
        size_t components = 0;

        Indexed4AryHeap heap(numVertices);

        for (VertexId startVertex = 0; startVertex < static_cast<VertexId>(numVertices); ++startVertex) {
            if (inTree[startVertex]) {
                continue;
            }

            ++components;
            inTree[startVertex] = true;
            heap.reset(numVertices);

            for (const auto& arc : graph.neighbors(startVertex)) {
                sink.onEdgeConsidered(arc.id, startVertex, arc.target, arc.cost);
                heap.decreaseKey(arc.target, arc.cost, arc.id);
            }

            while (!heap.empty()) {
                const HeapNode minNode = heap.pop();
                const VertexId u = minNode.vertex;
                const EdgeId bestEdge = minNode.bestEdgeId;

                inTree[u] = true;
                acceptedEdges.push_back(bestEdge);
                totalCost += minNode.key;

                const auto& edgeObj = graph.edge(bestEdge);
                sink.onEdgeAccepted(bestEdge, edgeObj.u, edgeObj.v, edgeObj.cost);

                for (const auto& arc : graph.neighbors(u)) {
                    if (inTree[arc.target]) {
                        sink.onEdgeRejected(arc.id, u, arc.target, arc.cost, "Endpoints already connected");
                    } else {
                        sink.onEdgeConsidered(arc.id, u, arc.target, arc.cost);
                        heap.decreaseKey(arc.target, arc.cost, arc.id);
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

std::unique_ptr<IMstAlgorithm> makePrimHeap() {
    return std::make_unique<PrimHeapAlgorithm>();
}

} // namespace netopt
