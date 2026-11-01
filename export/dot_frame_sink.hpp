#pragma once

#include "algo/mst_algorithm.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace netopt {

enum class EdgeVisualState {
    Unseen,
    Candidate,
    Accepted,
    Rejected
};

class DotFrameSink : public ITraceSink {
public:
    DotFrameSink(const Graph& graph,
                 std::string outputDir,
                 std::string prefix = "frame_");

    void onEdgeConsidered(EdgeId edgeId, VertexId u, VertexId v, Cost cost) override;
    void onEdgeAccepted(EdgeId edgeId, VertexId u, VertexId v, Cost cost) override;
    void onEdgeRejected(EdgeId edgeId, VertexId u, VertexId v, Cost cost, const std::string& reason) override;

    [[nodiscard]] size_t frameCount() const noexcept { return frameIndex_; }
    [[nodiscard]] const std::string& outputDir() const noexcept { return outputDir_; }

private:
    void writeFrame(const std::string& eventLabel);

    const Graph& graph_;
    std::string outputDir_;
    std::string prefix_;
    size_t frameIndex_{0};
    std::vector<EdgeVisualState> edgeStates_;
    std::vector<std::string> rejectReasons_;
};

} // namespace netopt
