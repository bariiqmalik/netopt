#include "io/graph_repository.hpp"
#include "io/csv_tokenizer.hpp"
#include "io/row_validator.hpp"
#include "core/graph_builder.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace netopt {

Result<std::string> CsvGraphRepository::readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::err("Failed to open file: " + filepath);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return Result<std::string>::ok(ss.str());
}

Result<void> CsvGraphRepository::atomicWriteFile(const std::string& filepath, const std::string& content) {
    const std::string tmpPath = filepath + ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return Result<void>::err("Failed to open temporary file for writing: " + tmpPath);
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!file.good()) {
            return Result<void>::err("Failed while writing content to file: " + tmpPath);
        }
        file.flush();
    }

    std::error_code ec;
    // std::filesystem::rename overwrites existing file on POSIX, but on Windows rename fails if target exists.
    // std::filesystem::copy_file with overwrite_existing or remove target first ensures atomic swap.
    if (std::filesystem::exists(filepath, ec)) {
        std::filesystem::remove(filepath, ec);
    }
    std::filesystem::rename(tmpPath, filepath, ec);
    if (ec) {
        return Result<void>::err("Failed to replace destination file atomically: " + ec.message());
    }

    return Result<void>::ok();
}

Result<Graph> CsvGraphRepository::load(const std::string& verticesPath,
                                      const std::string& edgesPath,
                                      DiagnosticsCollector& diagnostics) {
    auto vContent = readFile(verticesPath);
    if (vContent.isErr()) {
        diagnostics.addError(verticesPath, 0, vContent.error());
        return Result<Graph>::err(vContent.error());
    }

    auto eContent = readFile(edgesPath);
    if (eContent.isErr()) {
        diagnostics.addError(edgesPath, 0, eContent.error());
        return Result<Graph>::err(eContent.error());
    }

    auto vRows = CsvTokenizer::tokenize(vContent.value(), verticesPath, diagnostics);
    if (vRows.isErr()) {
        return Result<Graph>::err("Failed to tokenize vertices file");
    }

    auto eRows = CsvTokenizer::tokenize(eContent.value(), edgesPath, diagnostics);
    if (eRows.isErr()) {
        return Result<Graph>::err("Failed to tokenize edges file");
    }

    GraphBuilder builder;
    auto vValRes = RowValidator::validateAndLoadVertices(vRows.value(), builder, verticesPath, diagnostics);
    if (vValRes.isErr() || diagnostics.hasErrors()) {
        return Result<Graph>::err("Vertex validation failed");
    }

    auto eValRes = RowValidator::validateAndLoadEdges(eRows.value(), builder, edgesPath, diagnostics);
    if (eValRes.isErr() || diagnostics.hasErrors()) {
        return Result<Graph>::err("Edge validation failed");
    }

    return builder.build(diagnostics, edgesPath);
}

Result<void> CsvGraphRepository::save(const Graph& graph,
                                     const std::string& verticesPath,
                                     const std::string& edgesPath) {
    // Generate vertices.csv content
    std::ostringstream vss;
    vss << "#netopt-schema:1\n";
    vss << "id,label,type,x,y\n";
    for (size_t i = 0; i < graph.vertexCount(); ++i) {
        const auto& v = graph.vertex(static_cast<VertexId>(i));
        vss << v.externalId << ","
            << (v.label.find(',') != std::string::npos ? ("\"" + v.label + "\"") : v.label) << ","
            << v.type << ","
            << v.x << ","
            << v.y << "\n";
    }

    auto vRes = atomicWriteFile(verticesPath, vss.str());
    if (vRes.isErr()) {
        return vRes;
    }

    // Generate edges.csv content
    std::ostringstream ess;
    ess << "#netopt-schema:1\n";
    ess << "source,target,cost,length_m,medium\n";
    for (size_t i = 0; i < graph.edgeCount(); ++i) {
        const auto& e = graph.edge(static_cast<EdgeId>(i));
        const auto& m = graph.edgeMetadata(static_cast<EdgeId>(i));
        const auto& u = graph.vertex(e.u);
        const auto& v = graph.vertex(e.v);
        ess << u.externalId << ","
            << v.externalId << ","
            << e.cost << ","
            << m.length_m << ","
            << m.medium << "\n";
    }

    return atomicWriteFile(edgesPath, ess.str());
}

Result<void> CsvGraphRepository::saveMstResult(const Graph& graph,
                                             const MstResult& result,
                                             const std::string& filepath) {
    std::ostringstream ss;
    ss << "#netopt-schema:1\n";
    ss << "source,target,cost,length_m,medium,edge_id\n";
    for (const auto edgeId : result.edges) {
        const auto& e = graph.edge(edgeId);
        const auto& m = graph.edgeMetadata(edgeId);
        const auto& u = graph.vertex(e.u);
        const auto& v = graph.vertex(e.v);
        ss << u.externalId << ","
           << v.externalId << ","
           << e.cost << ","
           << m.length_m << ","
           << m.medium << ","
           << e.id << "\n";
    }
    return atomicWriteFile(filepath, ss.str());
}

Result<void> CsvGraphRepository::saveReport(const std::string& reportText,
                                          const std::string& filepath) {
    return atomicWriteFile(filepath, reportText);
}

} // namespace netopt
