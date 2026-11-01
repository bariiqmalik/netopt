#include "ui/menu.hpp"
#include "algo/mst_algorithm.hpp"
#include "io/graph_repository.hpp"
#include "analysis/verifier.hpp"
#include "analysis/sensitivity.hpp"
#include "export/dot_exporter.hpp"
#include "export/dot_frame_sink.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // Check for replay mode: netopt --replay <v.csv> <e.csv> <output_dir>
    if (argc >= 5 && std::string(argv[1]) == "--replay") {
        std::string vFile = argv[2];
        std::string eFile = argv[3];
        std::string outDir = argv[4];

        netopt::CsvGraphRepository repo;
        netopt::DiagnosticsCollector diag;
        auto gRes = repo.load(vFile, eFile, diag);
        if (gRes.isErr()) {
            std::cerr << "Failed to load graph: " << gRes.error() << "\n";
            return 1;
        }

        const auto& g = gRes.value();
        std::cout << "Recording replay frames into " << outDir << "...\n";
        netopt::DotFrameSink frameSink(g, outDir, "frame_");
        auto kruskal = netopt::AlgorithmFactory::create("Kruskal");
        auto mst = kruskal->run(g, frameSink);

        std::cout << "Generated " << frameSink.frameCount() << " frames for MST cost " << mst.totalCost << ".\n";
        return 0;
    }

    // Check for export mode: netopt --export-dot <v.csv> <e.csv> <out.dot>
    if (argc >= 5 && std::string(argv[1]) == "--export-dot") {
        std::string vFile = argv[2];
        std::string eFile = argv[3];
        std::string dotFile = argv[4];

        netopt::CsvGraphRepository repo;
        netopt::DiagnosticsCollector diag;
        auto gRes = repo.load(vFile, eFile, diag);
        if (gRes.isErr()) return 1;

        const auto& g = gRes.value();
        auto kruskal = netopt::AlgorithmFactory::create("Kruskal");
        netopt::NullSink sink;
        auto mst = kruskal->run(g, sink);
        auto rep = netopt::SensitivityAnalyzer::analyze(g, mst);

        netopt::DotExporter::exportMst(g, mst, dotFile, rep.criticalBridgeEdges);
        std::cout << "Exported MST DOT to " << dotFile << "\n";
        return 0;
    }

    // If 2 arguments: netopt <v.csv> <e.csv>
    if (argc >= 3) {
        std::string vFile = argv[1];
        std::string eFile = argv[2];
        std::cout << "Loading network headlessly from " << vFile << " and " << eFile << "...\n";

        netopt::CsvGraphRepository repo;
        netopt::DiagnosticsCollector diag;
        auto gRes = repo.load(vFile, eFile, diag);

        if (diag.hasWarnings()) {
            std::cout << diag.formatAll();
        }
        if (gRes.isErr() || diag.hasErrors()) {
            std::cerr << "Failed to load network:\n" << diag.formatAll();
            return 1;
        }

        const auto& g = gRes.value();
        std::cout << "Loaded " << g.vertexCount() << " vertices, " << g.edgeCount() << " edges.\n";

        auto kruskal = netopt::AlgorithmFactory::create("Kruskal");
        netopt::NullSink sink;
        auto mst = kruskal->run(g, sink);

        std::cout << "MST Total Cost: " << mst.totalCost << "\n";
        std::cout << "Components: " << mst.components << "\n";

        auto ver = netopt::Verifier::verify(g, mst);
        std::cout << "Verification: " << ver.summary() << "\n";

        auto sens = netopt::SensitivityAnalyzer::analyze(g, mst);
        std::cout << "\n" << sens.formatConsoleTable(g);
        return 0;
    }

    // Default interactive mode
    netopt::Menu menu;
    menu.run();
    return 0;
}
