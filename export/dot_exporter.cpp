#include "export/dot_exporter.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iomanip>

namespace netopt {

bool DotExporter::isDotAvailable() {
#ifdef _WIN32
    return std::system("where dot >nul 2>nul") == 0;
#else
    return std::system("which dot >/dev/null 2>&1") == 0;
#endif
}

Result<void> DotExporter::exportGraph(const Graph& graph,
                                      const std::string& dotFilePath,
                                      const std::string& title) {
    std::ofstream out(dotFilePath);
    if (!out.is_open()) {
        return Result<void>::err("Failed to open file for DOT export: " + dotFilePath);
    }

    out << "graph Network {\n";
    out << "  label=\"" << title << "\";\n";
    out << "  labelloc=\"t\";\n";
    out << "  node [shape=box, style=\"filled,rounded\", fillcolor=\"#E3F2FD\", fontname=\"Helvetica\"];\n";
    out << "  edge [fontname=\"Helvetica\", fontsize=10];\n\n";

    for (size_t i = 0; i < graph.vertexCount(); ++i) {
        const auto& v = graph.vertex(static_cast<VertexId>(i));
        out << "  " << v.externalId << " [label=\"" << v.label << "\\n(" << v.type << ")\"];\n";
    }
    out << "\n";

    for (const auto& e : graph.edges()) {
        const auto& u = graph.vertex(e.u);
        const auto& v = graph.vertex(e.v);
        out << "  " << u.externalId << " -- " << v.externalId
            << " [label=\"" << e.cost << "\", color=\"#90A4AE\"];\n";
    }

    out << "}\n";
    return Result<void>::ok();
}

Result<void> DotExporter::exportMst(const Graph& graph,
                                    const MstResult& mst,
                                    const std::string& dotFilePath,
                                    const std::unordered_set<EdgeId>& criticalEdges,
                                    const std::string& title) {
    std::ofstream out(dotFilePath);
    if (!out.is_open()) {
        return Result<void>::err("Failed to open file for DOT export: " + dotFilePath);
    }

    std::unordered_set<EdgeId> mstSet(mst.edges.begin(), mst.edges.end());

    out << "graph MST {\n";
    out << "  label=\"" << title << "\\nTotal Cost: " << mst.totalCost << "\";\n";
    out << "  labelloc=\"t\";\n";
    out << "  node [shape=box, style=\"filled,rounded\", fillcolor=\"#E8F5E9\", fontname=\"Helvetica\"];\n";
    out << "  edge [fontname=\"Helvetica\", fontsize=10];\n\n";

    for (size_t i = 0; i < graph.vertexCount(); ++i) {
        const auto& v = graph.vertex(static_cast<VertexId>(i));
        out << "  " << v.externalId << " [label=\"" << v.label << "\"];\n";
    }
    out << "\n";

    for (const auto& e : graph.edges()) {
        const auto& u = graph.vertex(e.u);
        const auto& v = graph.vertex(e.v);

        if (mstSet.contains(e.id)) {
            if (criticalEdges.contains(e.id)) {
                // Critical bridge edge: bold dark orange / red
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << " (CRITICAL)\", color=\"#D32F2F\", penwidth=3.0];\n";
            } else {
                // Standard MST edge: bold dark green
                out << "  " << u.externalId << " -- " << v.externalId
                    << " [label=\"" << e.cost << "\", color=\"#2E7D32\", penwidth=2.5];\n";
            }
        } else {
            // Non-tree edge: dashed light gray
            out << "  " << u.externalId << " -- " << v.externalId
                << " [label=\"" << e.cost << "\", color=\"#CFD8DC\", style=\"dashed\"];\n";
        }
    }

    out << "}\n";
    return Result<void>::ok();
}

Result<std::string> DotExporter::renderToPng(const std::string& dotFilePath,
                                             const std::string& pngFilePath) {
    if (!isDotAvailable()) {
        return Result<std::string>::err("Graphviz 'dot' executable is not available on PATH.");
    }

    std::string cmd = "dot -Tpng \"" + dotFilePath + "\" -o \"" + pngFilePath + "\"";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        return Result<std::string>::err("Failed to execute dot command: " + cmd);
    }

    return Result<std::string>::ok(pngFilePath);
}

} // namespace netopt
