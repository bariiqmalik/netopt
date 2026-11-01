#include "export/dot_frame_sink.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace netopt {

DotFrameSink::DotFrameSink(const Graph& graph,
                           std::string outputDir,
                           std::string prefix)
    : graph_(graph),
      outputDir_(std::move(outputDir)),
      prefix_(std::move(prefix)),
      frameIndex_(0),
      runningCost_(0),
      edgeStates_(graph.edgeCount(), EdgeVisualState::Unseen),
      rejectReasons_(graph.edgeCount(), "") {
    std::error_code ec;
    std::filesystem::create_directories(outputDir_, ec);
}

DotFrameSink::~DotFrameSink() {
    writeSidecarJson();
}

void DotFrameSink::writeFrame(const std::string& eventLabel,
                              const std::string& eventType,
                              EdgeId eId,
                              VertexId u,
                              VertexId v,
                              Cost cost) {
    ++frameIndex_;
    std::ostringstream fileOss;
    fileOss << prefix_ << std::setfill('0') << std::setw(3) << frameIndex_ << ".dot";
    const std::string filename = fileOss.str();
    const std::string fullPath = outputDir_ + "/" + filename;

    framesMetadata_.push_back(FrameMetadata{
        .index = frameIndex_,
        .eventType = eventType,
        .edgeId = eId,
        .u = u,
        .v = v,
        .edgeCost = cost,
        .runningCost = runningCost_,
        .label = eventLabel
    });

    std::ofstream out(fullPath);
    if (!out.is_open()) {
        return;
    }

    out << "graph Frame {\n";
    out << "  label=\"" << eventLabel << "\\nRunning Cost: " << runningCost_
        << " | Step " << frameIndex_ << "\";\n";
    out << "  labelloc=\"t\";\n";
    out << "  node [shape=box, style=\"filled,rounded\", fillcolor=\"#ECEFF1\", fontname=\"Helvetica\"];\n";
    out << "  edge [fontname=\"Helvetica\", fontsize=10];\n\n";

    for (size_t i = 0; i < graph_.vertexCount(); ++i) {
        const auto& vert = graph_.vertex(static_cast<VertexId>(i));
        out << "  " << vert.externalId << " [label=\"" << vert.label << "\"];\n";
    }
    out << "\n";

    for (size_t i = 0; i < graph_.edgeCount(); ++i) {
        const auto& e = graph_.edge(static_cast<EdgeId>(i));
        const auto& vertU = graph_.vertex(e.u);
        const auto& vertV = graph_.vertex(e.v);
        const auto state = edgeStates_[i];

        switch (state) {
            case EdgeVisualState::Accepted:
                out << "  " << vertU.externalId << " -- " << vertV.externalId
                    << " [label=\"" << e.cost << "\", color=\"#2E7D32\", penwidth=3.0];\n";
                break;
            case EdgeVisualState::Rejected:
                out << "  " << vertU.externalId << " -- " << vertV.externalId
                    << " [label=\"" << e.cost << " (REJECTED)\", color=\"#D32F2F\", penwidth=1.5, style=\"dashed\"];\n";
                break;
            case EdgeVisualState::Candidate:
                out << "  " << vertU.externalId << " -- " << vertV.externalId
                    << " [label=\"" << e.cost << "\", color=\"#F57C00\", penwidth=2.5];\n";
                break;
            case EdgeVisualState::Unseen:
                out << "  " << vertU.externalId << " -- " << vertV.externalId
                    << " [label=\"" << e.cost << "\", color=\"#CFD8DC\", style=\"dotted\"];\n";
                break;
        }
    }

    out << "}\n";
}

void DotFrameSink::writeSidecarJson() const {
    const std::string jsonPath = outputDir_ + "/frames.json";
    std::ofstream out(jsonPath);
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"frameCount\": " << frameIndex_ << ",\n";
    out << "  \"finalCost\": " << runningCost_ << ",\n";
    out << "  \"frames\": [\n";

    for (size_t i = 0; i < framesMetadata_.size(); ++i) {
        const auto& f = framesMetadata_[i];
        std::ostringstream fileOss;
        fileOss << prefix_ << std::setfill('0') << std::setw(3) << f.index << ".dot";

        out << "    {\n";
        out << "      \"index\": " << f.index << ",\n";
        out << "      \"filename\": \"" << fileOss.str() << "\",\n";
        out << "      \"eventType\": \"" << f.eventType << "\",\n";
        out << "      \"edgeId\": " << f.edgeId << ",\n";
        out << "      \"edgeCost\": " << f.edgeCost << ",\n";
        out << "      \"runningCost\": " << f.runningCost << ",\n";
        out << "      \"label\": \"";
        for (char c : f.label) {
            if (c == '"') out << "\\\"";
            else if (c == '\\') out << "\\\\";
            else out << c;
        }
        out << "\"\n";
        out << "    }" << (i + 1 < framesMetadata_.size() ? "," : "") << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

void DotFrameSink::onEdgeConsidered(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
    if (edgeId < edgeStates_.size() && edgeStates_[edgeId] != EdgeVisualState::Accepted) {
        edgeStates_[edgeId] = EdgeVisualState::Candidate;
    }
    std::ostringstream ss;
    ss << "Considering edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "), cost=" << cost;
    writeFrame(ss.str(), "considered", edgeId, u, v, cost);
}

void DotFrameSink::onEdgeAccepted(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
    if (edgeId < edgeStates_.size()) {
        edgeStates_[edgeId] = EdgeVisualState::Accepted;
    }
    runningCost_ += cost;
    std::ostringstream ss;
    ss << "ACCEPTED edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "), cost=" << cost;
    writeFrame(ss.str(), "accepted", edgeId, u, v, cost);
}

void DotFrameSink::onEdgeRejected(EdgeId edgeId, VertexId u, VertexId v, Cost cost, const std::string& reason) {
    if (edgeId < edgeStates_.size()) {
        edgeStates_[edgeId] = EdgeVisualState::Rejected;
        rejectReasons_[edgeId] = reason;
    }
    std::ostringstream ss;
    ss << "REJECTED edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "): " << reason;
    writeFrame(ss.str(), "rejected", edgeId, u, v, cost);
}

} // namespace netopt
