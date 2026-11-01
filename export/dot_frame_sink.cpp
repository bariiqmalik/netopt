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
      edgeStates_(graph.edgeCount(), EdgeVisualState::Unseen),
      rejectReasons_(graph.edgeCount(), "") {
    std::error_code ec;
    std::filesystem::create_directories(outputDir_, ec);
}

void DotFrameSink::writeFrame(const std::string& eventLabel) {
    ++frameIndex_;
    std::ostringstream pathOss;
    pathOss << outputDir_ << "/" << prefix_ << std::setfill('0') << std::setw(3) << frameIndex_ << ".dot";

    std::ofstream out(pathOss.str());
    if (!out.is_open()) {
        return;
    }

    out << "graph Frame {\n";
    out << "  label=\"" << eventLabel << "\\nStep " << frameIndex_ << "\";\n";
    out << "  labelloc=\"t\";\n";
    out << "  node [shape=box, style=\"filled,rounded\", fillcolor=\"#ECEFF1\", fontname=\"Helvetica\"];\n";
    out << "  edge [fontname=\"Helvetica\", fontsize=10];\n\n";

    for (size_t i = 0; i < graph_.vertexCount(); ++i) {
        const auto& v = graph_.vertex(static_cast<VertexId>(i));
        out << "  " << v.externalId << " [label=\"" << v.label << "\"];\n";
    }
    out << "\n";

    for (size_t i = 0; i < graph_.edgeCount(); ++i) {
        const auto& e = graph_.edge(static_cast<EdgeId>(i));
        const auto& u = graph_.vertex(e.u);
        const auto& v = graph_.vertex(e.v);
        const auto state = edgeStates_[i];

        switch (state) {
            case EdgeVisualState::Accepted:
                // Accepted = bold green
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << "\", color=\"#2E7D32\", penwidth=3.0];\n";
                break;
            case EdgeVisualState::Rejected:
                // Rejected = red dashed
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << " (REJECTED)\", color=\"#D32F2F\", penwidth=1.5, style=\"dashed\"];\n";
                break;
            case EdgeVisualState::Candidate:
                // Candidate = solid gray / amber highlight
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << "\", color=\"#F57C00\", penwidth=2.5];\n";
                break;
            case EdgeVisualState::Unseen:
                // Unseen = light gray dotted
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << "\", color=\"#CFD8DC\", style=\"dotted\"];\n";
                break;
        }
    }

    out << "}\n";
}

void DotFrameSink::onEdgeConsidered(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
    if (edgeId < edgeStates_.size() && edgeStates_[edgeId] != EdgeVisualState::Accepted) {
        edgeStates_[edgeId] = EdgeVisualState::Candidate;
    }
    std::ostringstream ss;
    ss << "Considering edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "), cost=" << cost;
    writeFrame(ss.str());
}

void DotFrameSink::onEdgeAccepted(EdgeId edgeId, VertexId u, VertexId v, Cost cost) {
    if (edgeId < edgeStates_.size()) {
        edgeStates_[edgeId] = EdgeVisualState::Accepted;
    }
    std::ostringstream ss;
    ss << "ACCEPTED edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "), cost=" << cost;
    writeFrame(ss.str());
}

void DotFrameSink::onEdgeRejected(EdgeId edgeId, VertexId u, VertexId v, Cost cost, const std::string& reason) {
    if (edgeId < edgeStates_.size()) {
        edgeStates_[edgeId] = EdgeVisualState::Rejected;
        rejectReasons_[edgeId] = reason;
    }
    std::ostringstream ss;
    ss << "REJECTED edge (" << graph_.vertex(u).label << " - "
       << graph_.vertex(v).label << "), cost=" << cost << ": " << reason;
    writeFrame(ss.str());
}

} // namespace netopt
