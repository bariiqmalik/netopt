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

struct FrameMetadata {
    size_t index{0};
    std::string eventType; // "considered", "accepted", "rejected"
    EdgeId edgeId{0};
    VertexId u{0};
    VertexId v{0};
    Cost edgeCost{0};
    Cost runningCost{0};
    std::string label;
};

class DotFrameSink : public ITraceSink {
public:
    DotFrameSink(const Graph& graph,
                 std::string outputDir,
                 std::string prefix = "frame_");

    ~DotFrameSink() override;

    void onEdgeConsidered(EdgeId edgeId, VertexId u, VertexId v, Cost cost) override;
    void onEdgeAccepted(EdgeId edgeId, VertexId u, VertexId v, Cost cost) override;
    void onEdgeRejected(EdgeId edgeId, VertexId u, VertexId v, Cost cost, const std::string& reason) override;

    [[nodiscard]] size_t frameCount() const noexcept { return frameIndex_; }
    [[nodiscard]] Cost runningCost() const noexcept { return runningCost_; }
    [[nodiscard]] const std::string& outputDir() const noexcept { return outputDir_; }
    [[nodiscard]] const std::vector<FrameMetadata>& metadata() const noexcept { return framesMetadata_; }

private:
    void writeFrame(const std::string& eventLabel, const std::string& eventType, EdgeId eId, VertexId u, VertexId v, Cost cost);
    void writeSidecarJson() const;

    const Graph& graph_;
    std::string outputDir_;
    std::string prefix_;
    size_t frameIndex_{0};
    Cost runningCost_{0};
    std::vector<EdgeVisualState> edgeStates_;
    std::vector<std::string> rejectReasons_;
    std::vector<FrameMetadata> framesMetadata_;
};

} // namespace netopt
