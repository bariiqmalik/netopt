#include "algo/mst_algorithm.hpp"

namespace netopt {

std::unique_ptr<IMstAlgorithm> makeKruskal();
std::unique_ptr<IMstAlgorithm> makePrimHeap();
std::unique_ptr<IMstAlgorithm> makePrimDense();

std::unique_ptr<IMstAlgorithm> AlgorithmFactory::create(const std::string& algorithmName) {
    if (algorithmName == "Kruskal" || algorithmName == "kruskal") {
        return makeKruskal();
    }
    if (algorithmName == "Prim (Heap)" || algorithmName == "prim" || algorithmName == "Prim") {
        return makePrimHeap();
    }
    if (algorithmName == "Prim (Dense)" || algorithmName == "prim_dense" || algorithmName == "PrimDense") {
        return makePrimDense();
    }
    return nullptr;
}

std::unique_ptr<IMstAlgorithm> AlgorithmFactory::createAuto(const Graph& graph, double densityThreshold) {
    if (graph.density() >= densityThreshold) {
        return makePrimDense();
    }
    return makePrimHeap();
}

std::vector<std::string> AlgorithmFactory::availableAlgorithms() {
    return {"Kruskal", "Prim (Heap)", "Prim (Dense)"};
}

} // namespace netopt
