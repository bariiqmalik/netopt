#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include <string>

namespace netopt {

struct VerificationReport {
    bool valid{true};
    size_t expectedEdges{0};
    size_t actualEdges{0};
    size_t components{0};
    std::string failureReason;

    [[nodiscard]] std::string summary() const {
        if (valid) {
            return "Verified optimal (passed V-c edge count, acyclicity, spanning, and cycle optimality properties)";
        }
        return "Verification failed: " + failureReason;
    }
};

class Verifier {
public:
    static VerificationReport verify(const Graph& graph, const MstResult& result);

private:
    static bool findPathMax(VertexId curr,
                            VertexId target,
                            VertexId parent,
                            const std::vector<std::vector<std::pair<VertexId, Edge>>>& treeAdj,
                            Edge& outMaxEdge);
};

} // namespace netopt
