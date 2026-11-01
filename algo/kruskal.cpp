#include "algo/mst_algorithm.hpp"
#include "core/dsu.hpp"
#include <algorithm>

namespace netopt {

class KruskalAlgorithm : public IMstAlgorithm {
public:
    [[nodiscard]] std::string name() const override {
        return "Kruskal";
    }

    MstResult run(const Graph& graph, ITraceSink& sink) override {
        const size_t numVertices = graph.vertexCount();
        if (numVertices == 0) {
            return MstResult{.edges = {}, .totalCost = 0, .components = 0};
        }

        std::vector<Edge> sortedEdges = graph.edges();
        std::sort(sortedEdges.begin(), sortedEdges.end());

        DisjointSet dsu(numVertices);
        std::vector<EdgeId> acceptedEdges;
        acceptedEdges.reserve(numVertices > 0 ? numVertices - 1 : 0);
        Cost totalCost = 0;

        std::vector<bool> visited(numVertices, false);
        size_t originalComponents = 0;
        std::vector<VertexId> queue;
        for (VertexId i = 0; i < numVertices; ++i) {
            if (!visited[i]) {
                ++originalComponents;
                visited[i] = true;
                queue.push_back(i);
                while (!queue.empty()) {
                    const VertexId curr = queue.back();
                    queue.pop_back();
                    for (const auto& arc : graph.neighbors(curr)) {
                        if (!visited[arc.target]) {
                            visited[arc.target] = true;
                            queue.push_back(arc.target);
                        }
                    }
                }
            }
        }

        for (const auto& edge : sortedEdges) {
            sink.onEdgeConsidered(edge.id, edge.u, edge.v, edge.cost);

            if (dsu.unite(edge.u, edge.v)) {
                acceptedEdges.push_back(edge.id);
                totalCost += edge.cost;
                sink.onEdgeAccepted(edge.id, edge.u, edge.v, edge.cost);

                if (dsu.components() == originalComponents) {
                    break;
                }
            } else {
                sink.onEdgeRejected(edge.id, edge.u, edge.v, edge.cost, "Cycle detected");
            }
        }

        std::sort(acceptedEdges.begin(), acceptedEdges.end());

        return MstResult{
            .edges = std::move(acceptedEdges),
            .totalCost = totalCost,
            .components = dsu.components()
        };
    }
};

std::unique_ptr<IMstAlgorithm> makeKruskal() {
    return std::make_unique<KruskalAlgorithm>();
}

} // namespace netopt
