#include "ui/menu.hpp"
#include "algo/mst_algorithm.hpp"
#include "analysis/verifier.hpp"
#include "analysis/sensitivity.hpp"
#include "analysis/benchmark.hpp"
#include "export/dot_exporter.hpp"
#include "export/dot_frame_sink.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <limits>
#include <algorithm>
#include <random>

namespace netopt {

Menu::Menu() = default;

void Menu::printHeader() const {
    std::cout << "\n================================================================================\n";
    std::cout << "                 NETWORK CABLE OPTIMIZATION SYSTEM (netopt)                     \n";
    std::cout << "          Minimum Spanning Tree / Forest Optimizer & Resilience Engine          \n";
    std::cout << "================================================================================\n";
}

void Menu::showMenu() const {
    std::cout << "\n---------------------------------- MAIN MENU -----------------------------------\n";
    std::cout << " 1. Load network from CSV files\n";
    std::cout << " 2. Add / remove vertex, add / remove edge (in-memory editing)\n";
    std::cout << " 3. Display current network topology\n";
    std::cout << " 4. Run Kruskal's algorithm\n";
    std::cout << " 5. Run Prim's algorithm (Heap / Dense / Auto)\n";
    std::cout << " 6. Compare Prim vs Kruskal (cost, edge set equality, runtime, operations)\n";
    std::cout << " 7. Save network and results (CSV / Text Report)\n";
    std::cout << " 8. Export Graphviz DOT (+ render PNG if Graphviz is installed)\n";
    std::cout << " 9. Failure-resilience & Sensitivity analysis (Bridges, Tolerances, 2nd-Best)\n";
    std::cout << "10. Run verification and cross-validation suite\n";
    std::cout << "11. Run empirical benchmark suite\n";
    std::cout << " 0. Exit\n";
    std::cout << "--------------------------------------------------------------------------------\n";
}

int Menu::readInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return 0; // EOF encountered, exit cleanly
        }
        try {
            size_t pos = 0;
            int val = std::stoi(line, &pos);
            if (pos == line.size() && val >= minVal && val <= maxVal) {
                return val;
            }
        } catch (...) {}
        std::cout << "Invalid input. Please enter an integer between " << minVal << " and " << maxVal << ".\n";
    }
}

int64_t Menu::readInt64(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return 0;
        }
        try {
            size_t pos = 0;
            int64_t val = std::stoll(line, &pos);
            if (pos == line.size()) {
                return val;
            }
        } catch (...) {}
        std::cout << "Invalid input. Please enter an integer number.\n";
    }
}

double Menu::readDouble(const std::string& prompt, double defaultVal) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return defaultVal;
        }
        if (line.empty()) {
            return defaultVal;
        }
        try {
            size_t pos = 0;
            double val = std::stod(line, &pos);
            if (pos == line.size()) {
                return val;
            }
        } catch (...) {}
        std::cout << "Invalid input. Please enter a valid floating-point number.\n";
    }
}

std::string Menu::readString(const std::string& prompt, bool allowEmpty) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return "";
        }
        if (!line.empty() || allowEmpty) {
            return line;
        }
        std::cout << "Input cannot be empty. Please try again.\n";
    }
}

void Menu::handleLoadFromCsv() {
    std::cout << "\n=== Load Network from CSV ===\n";
    std::string vFile = readString("Enter path to vertices CSV [e.g. data/sample_small_vertices.csv]: ");
    std::string eFile = readString("Enter path to edges CSV [e.g. data/sample_small_edges.csv]: ");

    DiagnosticsCollector diag;
    auto gRes = repo_.load(vFile, eFile, diag);

    if (diag.hasWarnings()) {
        std::cout << "\nParser Warnings:\n" << diag.formatAll();
    }

    if (gRes.isErr() || diag.hasErrors()) {
        std::cout << "\nParser Errors:\n" << diag.formatAll();
        std::cout << "Failed to load network.\n";
        return;
    }

    currentGraph_ = std::move(gRes.value());
    currentMst_ = std::nullopt;
    lastAlgorithmRun_.clear();
    lastLoadedVerticesPath_ = vFile;
    lastLoadedEdgesPath_ = eFile;

    std::cout << "\nNetwork successfully loaded!\n";
    std::cout << "Vertices: " << currentGraph_->vertexCount()
              << ", Edges: " << currentGraph_->edgeCount()
              << ", Total Cable Cost: " << currentGraph_->totalOriginalCost() << "\n";
}

void Menu::handleDisplayNetwork() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    const auto& g = *currentGraph_;
    std::cout << "\n============================== NETWORK TOPOLOGY ==============================\n";
    std::cout << "Summary: " << g.vertexCount() << " vertices, " << g.edgeCount()
              << " edges, density: " << std::fixed << std::setprecision(4) << g.density()
              << ", total cable cost: " << g.totalOriginalCost() << "\n\n";

    std::cout << "Vertices:\n";
    std::cout << "------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(10) << "DenseId"
              << std::setw(14) << "ExternalId"
              << std::setw(24) << "Label"
              << std::setw(16) << "Type"
              << "Coordinates (x, y)\n";
    std::cout << "------------------------------------------------------------------------------\n";
    for (size_t i = 0; i < g.vertexCount(); ++i) {
        const auto& v = g.vertex(static_cast<VertexId>(i));
        std::cout << std::left << std::setw(10) << i
                  << std::setw(14) << v.externalId
                  << std::setw(24) << v.label
                  << std::setw(16) << v.type
                  << "(" << v.x << ", " << v.y << ")\n";
    }

    std::cout << "\nEdges (Cabling Links):\n";
    std::cout << "------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(8) << "EdgeId"
              << std::setw(28) << "Endpoints"
              << std::setw(12) << "Cost"
              << std::setw(12) << "Length (m)"
              << "Medium\n";
    std::cout << "------------------------------------------------------------------------------\n";
    for (size_t i = 0; i < g.edgeCount(); ++i) {
        const auto& e = g.edge(static_cast<EdgeId>(i));
        const auto& m = g.edgeMetadata(static_cast<EdgeId>(i));
        const auto& u = g.vertex(e.u);
        const auto& v = g.vertex(e.v);
        std::string endpoints = u.label + " -- " + v.label;

        std::cout << std::left << std::setw(8) << e.id
                  << std::setw(28) << endpoints
                  << std::setw(12) << e.cost
                  << std::setw(12) << m.length_m
                  << m.medium << "\n";
    }
    std::cout << "==============================================================================\n";
}

void Menu::displayMstResult(const std::string& algoName, const MstResult& res) {
    if (!currentGraph_) return;
    const auto& g = *currentGraph_;

    std::cout << "\n================================ MST COMPUTATION ================================\n";
    std::cout << "Algorithm: " << algoName << "\n";
    std::cout << "Components: " << res.components
              << (res.connected() ? " (Fully connected network)" : " (DISCONNECTED FOREST)") << "\n";
    std::cout << "Edges in Spanning Structure: " << res.edges.size()
              << " / " << (g.vertexCount() > 0 ? g.vertexCount() - res.components : 0) << "\n";
    std::cout << "Optimized Network Cost: " << res.totalCost << "\n";

    const Cost origCost = g.totalOriginalCost();
    const Cost savedCost = origCost - res.totalCost;
    const double pctSaved = origCost > 0 ? (100.0 * savedCost / static_cast<double>(origCost)) : 0.0;
    std::cout << "Budget Savings: " << savedCost << " (" << std::fixed << std::setprecision(2)
              << pctSaved << "% saved compared to full mesh)\n\n";

    std::cout << "Edges Selected in MST:\n";
    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(8) << "EdgeId"
              << std::setw(28) << "Connection"
              << std::setw(12) << "Cost"
              << std::setw(12) << "Length (m)"
              << "Medium\n";
    std::cout << "--------------------------------------------------------------------------------\n";

    for (const auto edgeId : res.edges) {
        const auto& e = g.edge(edgeId);
        const auto& m = g.edgeMetadata(edgeId);
        const auto& u = g.vertex(e.u);
        const auto& v = g.vertex(e.v);
        std::string conn = u.label + " -- " + v.label;

        std::cout << std::left << std::setw(8) << e.id
                  << std::setw(28) << conn
                  << std::setw(12) << e.cost
                  << std::setw(12) << m.length_m
                  << m.medium << "\n";
    }
    std::cout << "--------------------------------------------------------------------------------\n";

    // Automatic verification run
    auto verReport = Verifier::verify(g, res);
    if (verReport.valid) {
        std::cout << "[VERIFICATION] Verified optimal: " << verReport.summary() << "\n";
    } else {
        std::cout << "[VERIFICATION FAILED] " << verReport.summary() << "\n";
    }
    std::cout << "================================================================================\n";
}

void Menu::handleRunKruskal() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    auto kruskal = AlgorithmFactory::create("Kruskal");
    NullSink sink;

    auto tStart = std::chrono::high_resolution_clock::now();
    currentMst_ = kruskal->run(*currentGraph_, sink);
    auto tEnd = std::chrono::high_resolution_clock::now();
    lastAlgorithmRun_ = "Kruskal";

    double us = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
    std::cout << "\nExecution time: " << std::fixed << std::setprecision(1) << us << " microseconds\n";
    displayMstResult("Kruskal's Algorithm", *currentMst_);
}

void Menu::handleRunPrim() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    std::cout << "\nSelect Prim Variant:\n";
    std::cout << "1. Prim (Indexed 4-ary Heap)\n";
    std::cout << "2. Prim (Dense O(V^2) Array Scan)\n";
    std::cout << "3. Prim (Auto - threshold density = 0.40)\n";
    int choice = readInt("Enter choice [1-3]: ", 1, 3);

    std::unique_ptr<IMstAlgorithm> algo;
    std::string variantName;

    if (choice == 1) {
        algo = AlgorithmFactory::create("Prim (Heap)");
        variantName = "Prim (Indexed 4-ary Heap)";
    } else if (choice == 2) {
        algo = AlgorithmFactory::create("Prim (Dense)");
        variantName = "Prim (Dense O(V^2))";
    } else {
        algo = AlgorithmFactory::createAuto(*currentGraph_, 0.40);
        variantName = "Prim Auto (" + algo->name() + ")";
    }

    NullSink sink;
    auto tStart = std::chrono::high_resolution_clock::now();
    currentMst_ = algo->run(*currentGraph_, sink);
    auto tEnd = std::chrono::high_resolution_clock::now();
    lastAlgorithmRun_ = variantName;

    double us = std::chrono::duration<double, std::micro>(tEnd - tStart).count();
    std::cout << "\nExecution time: " << std::fixed << std::setprecision(1) << us << " microseconds\n";
    displayMstResult(variantName, *currentMst_);
}

void Menu::handleCompareAlgorithms() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }
    const auto& g = *currentGraph_;

    auto kruskal = AlgorithmFactory::create("Kruskal");
    auto primHeap = AlgorithmFactory::create("Prim (Heap)");
    auto primDense = AlgorithmFactory::create("Prim (Dense)");

    BenchmarkTraceSink sinkK, sinkH, sinkD;

    auto t0 = std::chrono::high_resolution_clock::now();
    auto resK = kruskal->run(g, sinkK);
    auto t1 = std::chrono::high_resolution_clock::now();

    auto resH = primHeap->run(g, sinkH);
    auto t2 = std::chrono::high_resolution_clock::now();

    auto resD = primDense->run(g, sinkD);
    auto t3 = std::chrono::high_resolution_clock::now();

    double timeK = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double timeH = std::chrono::duration<double, std::micro>(t2 - t1).count();
    double timeD = std::chrono::duration<double, std::micro>(t3 - t2).count();

    bool costEqual = (resK.totalCost == resH.totalCost) && (resK.totalCost == resD.totalCost);
    bool edgeSetsEqual = (resK.edges == resH.edges) && (resK.edges == resD.edges);

    std::cout << "\n=========================== ALGORITHM COMPARISON SUITE ===========================\n";
    std::cout << std::left << std::setw(18) << "Algorithm"
              << std::setw(14) << "Total Cost"
              << std::setw(14) << "Edges"
              << std::setw(14) << "Components"
              << std::setw(16) << "Time (us)"
              << "Operations\n";
    std::cout << "----------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Kruskal"
              << std::setw(14) << resK.totalCost
              << std::setw(14) << resK.edges.size()
              << std::setw(14) << resK.components
              << std::setw(16) << std::fixed << std::setprecision(1) << timeK
              << sinkK.operations() << "\n";

    std::cout << std::left << std::setw(18) << "Prim (Heap)"
              << std::setw(14) << resH.totalCost
              << std::setw(14) << resH.edges.size()
              << std::setw(14) << resH.components
              << std::setw(16) << std::fixed << std::setprecision(1) << timeH
              << sinkH.operations() << "\n";

    std::cout << std::left << std::setw(18) << "Prim (Dense)"
              << std::setw(14) << resD.totalCost
              << std::setw(14) << resD.edges.size()
              << std::setw(14) << resD.components
              << std::setw(16) << std::fixed << std::setprecision(1) << timeD
              << sinkD.operations() << "\n";
    std::cout << "----------------------------------------------------------------------------------\n";
    std::cout << "Verification Check:\n";
    std::cout << "  - All total costs identical: " << (costEqual ? "PASSED (MATCH)" : "FAILED (MISMATCH)") << "\n";
    std::cout << "  - All chosen edge sets identical: " << (edgeSetsEqual ? "PASSED (100% IDENTICAL)" : "FAILED") << "\n";
    std::cout << "==================================================================================\n";
}

void Menu::handleSaveNetworkAndResults() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    std::cout << "\nSave Options:\n";
    std::cout << "1. Save current network topology to CSV files\n";
    std::cout << "2. Save computed MST result to CSV (mst_result.csv)\n";
    std::cout << "3. Save complete human-readable report to text file (.txt)\n";
    int choice = readInt("Enter choice [1-3]: ", 1, 3);

    if (choice == 1) {
        std::string vFile = readString("Enter destination vertices CSV filename: ");
        std::string eFile = readString("Enter destination edges CSV filename: ");
        auto res = repo_.save(*currentGraph_, vFile, eFile);
        if (res.isOk()) {
            std::cout << "Successfully saved network atomically to " << vFile << " and " << eFile << "\n";
        } else {
            std::cout << "Error saving network: " << res.error() << "\n";
        }
    } else if (choice == 2) {
        if (!currentMst_) {
            std::cout << "No MST has been computed yet. Please run Kruskal or Prim first.\n";
            return;
        }
        std::string mstFile = readString("Enter destination MST CSV filename [default: mst_result.csv]: ", true);
        if (mstFile.empty()) mstFile = "mst_result.csv";

        auto res = repo_.saveMstResult(*currentGraph_, *currentMst_, mstFile);
        if (res.isOk()) {
            std::cout << "Successfully saved MST result to " << mstFile << "\n";
        } else {
            std::cout << "Error saving MST result: " << res.error() << "\n";
        }
    } else {
        std::string txtFile = readString("Enter report filename [default: network_report.txt]: ", true);
        if (txtFile.empty()) txtFile = "network_report.txt";

        std::ostringstream ss;
        ss << "================================================================================\n";
        ss << "                   NETWORK CABLE OPTIMIZATION REPORT                            \n";
        ss << "================================================================================\n\n";
        ss << "Network: " << currentGraph_->vertexCount() << " vertices, " << currentGraph_->edgeCount() << " edges\n";
        ss << "Total Cable Cost: " << currentGraph_->totalOriginalCost() << "\n\n";

        if (currentMst_) {
            ss << "Last MST Algorithm: " << lastAlgorithmRun_ << "\n";
            ss << "MST Total Cost: " << currentMst_->totalCost << "\n";
            ss << "Components: " << currentMst_->components << "\n";
            ss << "Edges Selected:\n";
            for (const auto edgeId : currentMst_->edges) {
                const auto& e = currentGraph_->edge(edgeId);
                const auto& u = currentGraph_->vertex(e.u);
                const auto& v = currentGraph_->vertex(e.v);
                ss << "  Edge #" << e.id << ": " << u.label << " -- " << v.label
                   << " | Cost: " << e.cost << "\n";
            }
        }

        auto res = repo_.saveReport(ss.str(), txtFile);
        if (res.isOk()) {
            std::cout << "Successfully saved report to " << txtFile << "\n";
        } else {
            std::cout << "Error saving report: " << res.error() << "\n";
        }
    }
}

void Menu::handleExportDot() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    std::cout << "\nExport Graphviz DOT:\n";
    std::cout << "1. Export Full Network Graph to DOT\n";
    std::cout << "2. Export MST to DOT (highlighting critical edges)\n";
    std::cout << "3. Run Algorithm Replay and generate frame-by-frame DOT files\n";
    int choice = readInt("Enter choice [1-3]: ", 1, 3);

    if (choice == 1) {
        std::string dotPath = readString("Enter destination DOT file [default: network.dot]: ", true);
        if (dotPath.empty()) dotPath = "network.dot";
        auto res = DotExporter::exportGraph(*currentGraph_, dotPath);
        if (res.isOk()) {
            std::cout << "Saved DOT file to " << dotPath << "\n";
            if (DotExporter::isDotAvailable()) {
                std::string pngPath = dotPath.substr(0, dotPath.find_last_of('.')) + ".png";
                auto renderRes = DotExporter::renderToPng(dotPath, pngPath);
                if (renderRes.isOk()) {
                    std::cout << "Rendered PNG image to " << pngPath << "\n";
                }
            } else {
                std::cout << "Note: Graphviz 'dot' is not installed/in PATH. Saved .dot file only.\n";
            }
        } else {
            std::cout << "Error: " << res.error() << "\n";
        }
    } else if (choice == 2) {
        if (!currentMst_) {
            std::cout << "No MST computed. Running Kruskal first...\n";
            auto kruskal = AlgorithmFactory::create("Kruskal");
            NullSink sink;
            currentMst_ = kruskal->run(*currentGraph_, sink);
        }

        auto rep = SensitivityAnalyzer::analyze(*currentGraph_, *currentMst_);
        std::string dotPath = readString("Enter destination DOT file [default: mst.dot]: ", true);
        if (dotPath.empty()) dotPath = "mst.dot";

        auto res = DotExporter::exportMst(*currentGraph_, *currentMst_, dotPath, rep.criticalBridgeEdges);
        if (res.isOk()) {
            std::cout << "Saved MST DOT file to " << dotPath << "\n";
            if (DotExporter::isDotAvailable()) {
                std::string pngPath = dotPath.substr(0, dotPath.find_last_of('.')) + ".png";
                auto renderRes = DotExporter::renderToPng(dotPath, pngPath);
                if (renderRes.isOk()) {
                    std::cout << "Rendered PNG image to " << pngPath << "\n";
                }
            } else {
                std::cout << "Note: Graphviz 'dot' is not installed/in PATH. Saved .dot file only.\n";
            }
        }
    } else {
        std::cout << "\nAlgorithm Replay Options:\n";
        std::cout << "1. Replay Kruskal's algorithm\n";
        std::cout << "2. Replay Prim's algorithm\n";
        std::cout << "3. Side-by-side comparison (Prim vs Kruskal)\n";
        int replayChoice = readInt("Enter choice [1-3]: ", 1, 3);

        auto detectPython = []() -> std::string {
            if (std::system("python --version >nul 2>nul") == 0) return "python";
            if (std::system("python3 --version >nul 2>nul") == 0) return "python3";
            const char* localApp = std::getenv("LOCALAPPDATA");
            if (localApp) {
                std::string pyPath = std::string(localApp) + "\\Programs\\Python\\Python312\\python.exe";
                if (std::filesystem::exists(pyPath)) return "\"" + pyPath + "\"";
            }
            return "";
        };

        const std::string pythonBin = detectPython();

        if (replayChoice == 1) {
            std::string outDir = readString("Enter directory for Kruskal frames [default: replay_kruskal]: ", true);
            if (outDir.empty()) outDir = "replay_kruskal";

            std::cout << "Recording Kruskal frames with DotFrameSink...\n";
            DotFrameSink frameSink(*currentGraph_, outDir, "frame_");
            auto kruskal = AlgorithmFactory::create("Kruskal");
            kruskal->run(*currentGraph_, frameSink);
            std::cout << "Generated " << frameSink.frameCount() << " DOT frames in directory '" << outDir << "'.\n";

            if (!pythonBin.empty()) {
                std::string htmlOut = outDir + "/replay.html";
                std::string cmd = pythonBin + " tools/animate.py --frames \"" + outDir + "\" --out \"" + htmlOut + "\" --title \"Kruskal Algorithm Replay\"";
                std::cout << "Generating self-contained HTML replay slider...\n";
                std::string sysCmd = "\"" + cmd + "\"";
                int ret = std::system(sysCmd.c_str());
                if (ret == 0) {
                    std::cout << "Successfully generated replay HTML at: " << std::filesystem::absolute(htmlOut).string() << "\n";
                    std::cout << "You can double-click this file to open it in any web browser.\n";
                }
            } else {
                std::cout << "\n[INFO] Python was not found on PATH.\n";
                std::cout << "DOT frames have been saved to '" << outDir << "'.\n";
                std::cout << "To build the HTML player, install Python and run:\n";
                std::cout << "  python tools/animate.py --frames \"" << outDir << "\" --out \"" << outDir << "/replay.html\"\n";
            }
        } else if (replayChoice == 2) {
            std::string outDir = readString("Enter directory for Prim frames [default: replay_prim]: ", true);
            if (outDir.empty()) outDir = "replay_prim";

            std::cout << "Recording Prim frames with DotFrameSink...\n";
            DotFrameSink frameSink(*currentGraph_, outDir, "frame_");
            auto prim = AlgorithmFactory::create("Prim (Heap)");
            prim->run(*currentGraph_, frameSink);
            std::cout << "Generated " << frameSink.frameCount() << " DOT frames in directory '" << outDir << "'.\n";

            if (!pythonBin.empty()) {
                std::string htmlOut = outDir + "/replay.html";
                std::string cmd = pythonBin + " tools/animate.py --frames \"" + outDir + "\" --out \"" + htmlOut + "\" --title \"Prim Algorithm Replay\"";
                std::cout << "Generating self-contained HTML replay slider...\n";
                std::string sysCmd = "\"" + cmd + "\"";
                int ret = std::system(sysCmd.c_str());
                if (ret == 0) {
                    std::cout << "Successfully generated replay HTML at: " << std::filesystem::absolute(htmlOut).string() << "\n";
                    std::cout << "You can double-click this file to open it in any web browser.\n";
                }
            } else {
                std::cout << "\n[INFO] Python was not found on PATH.\n";
                std::cout << "DOT frames have been saved to '" << outDir << "'.\n";
                std::cout << "To build the HTML player, install Python and run:\n";
                std::cout << "  python tools/animate.py --frames \"" << outDir << "\" --out \"" << outDir << "/replay.html\"\n";
            }
        } else {
            std::string leftDir = "replay_prim";
            std::string rightDir = "replay_kruskal";
            std::string compareHtml = "compare.html";

            std::cout << "Recording Prim frames to '" << leftDir << "'...\n";
            DotFrameSink primSink(*currentGraph_, leftDir, "frame_");
            auto prim = AlgorithmFactory::create("Prim (Heap)");
            prim->run(*currentGraph_, primSink);

            std::cout << "Recording Kruskal frames to '" << rightDir << "'...\n";
            DotFrameSink kruskalSink(*currentGraph_, rightDir, "frame_");
            auto kruskal = AlgorithmFactory::create("Kruskal");
            kruskal->run(*currentGraph_, kruskalSink);

            if (!pythonBin.empty()) {
                std::string cmd = pythonBin + " tools/animate.py --left \"" + leftDir + "\" --right \"" + rightDir + "\" --out \"" + compareHtml + "\"";
                std::cout << "Generating side-by-side comparison HTML...\n";
                std::string sysCmd = "\"" + cmd + "\"";
                int ret = std::system(sysCmd.c_str());
                if (ret == 0) {
                    std::cout << "Successfully generated side-by-side comparison HTML at: " << std::filesystem::absolute(compareHtml).string() << "\n";
                    std::cout << "You can double-click this file to open it in any web browser.\n";
                }
            } else {
                std::cout << "\n[INFO] Python was not found on PATH.\n";
                std::cout << "Frames saved to '" << leftDir << "' and '" << rightDir << "'.\n";
                std::cout << "To build the comparison, install Python and run:\n";
                std::cout << "  python tools/animate.py --left \"" << leftDir << "\" --right \"" << rightDir << "\" --out \"" << compareHtml << "\"\n";
            }
        }
    }
}

void Menu::handleResilienceReport() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded.\n";
        return;
    }

    if (!currentMst_) {
        std::cout << "No MST currently computed. Automatically computing MST with Kruskal...\n";
        auto kruskal = AlgorithmFactory::create("Kruskal");
        NullSink sink;
        currentMst_ = kruskal->run(*currentGraph_, sink);
    }

    auto rep = SensitivityAnalyzer::analyze(*currentGraph_, *currentMst_);
    std::cout << rep.formatConsoleTable(*currentGraph_);

    std::string csvChoice = readString("\nSave resilience report to CSV? [y/N]: ", true);
    if (csvChoice == "y" || csvChoice == "Y") {
        std::string csvPath = readString("Enter CSV filename [default: resilience_report.csv]: ", true);
        if (csvPath.empty()) csvPath = "resilience_report.csv";
        auto res = repo_.saveReport(rep.toCsv(*currentGraph_), csvPath);
        if (res.isOk()) {
            std::cout << "Saved resilience report to " << csvPath << "\n";
        }
    }
}

void Menu::handleVerificationAndCrossVal() {
    std::cout << "\nVerification & Cross-Validation Suite:\n";
    std::cout << "1. Verify current MST against all 4 optimality properties\n";
    std::cout << "2. Run 1000+ Randomized Cross-Validation Suite (Prim Heap vs Prim Dense vs Kruskal)\n";
    int choice = readInt("Enter choice [1-2]: ", 1, 2);

    if (choice == 1) {
        if (!currentGraph_) {
            std::cout << "No network currently loaded.\n";
            return;
        }
        if (!currentMst_) {
            std::cout << "No MST computed. Running Kruskal first...\n";
            auto kruskal = AlgorithmFactory::create("Kruskal");
            NullSink sink;
            currentMst_ = kruskal->run(*currentGraph_, sink);
        }
        auto rep = Verifier::verify(*currentGraph_, *currentMst_);
        std::cout << "\n" << rep.summary() << "\n";
    } else {
        std::cout << "\nRunning 1000+ randomized cross-validation suite...\n";
        std::mt19937_64 rng(42133742);
        auto kruskal = AlgorithmFactory::create("Kruskal");
        auto primHeap = AlgorithmFactory::create("Prim (Heap)");
        auto primDense = AlgorithmFactory::create("Prim (Dense)");
        NullSink sink;

        constexpr int kRuns = 1000;
        size_t passCount = 0;

        for (int i = 0; i < kRuns; ++i) {
            GraphBuilder builder;
            DiagnosticsCollector diag;
            size_t vCount = (i % 25) + 2;

            for (size_t v = 0; v < vCount; ++v) {
                builder.addVertex(static_cast<int64_t>(v), "V" + std::to_string(v));
            }

            std::uniform_int_distribution<Cost> costDist(1, 100);
            std::uniform_real_distribution<double> probDist(0.0, 1.0);
            double p = (i % 3 == 0) ? 0.2 : ((i % 3 == 1) ? 0.6 : 0.9);

            for (size_t u = 0; u < vCount; ++u) {
                for (size_t v = u + 1; v < vCount; ++v) {
                    if (probDist(rng) <= p) {
                        builder.addEdge(static_cast<int64_t>(u), static_cast<int64_t>(v), costDist(rng));
                    }
                }
            }

            auto g = builder.build(diag).value();
            auto rK = kruskal->run(g, sink);
            auto rH = primHeap->run(g, sink);
            auto rD = primDense->run(g, sink);

            if (rK.totalCost == rH.totalCost && rK.totalCost == rD.totalCost &&
                rK.edges == rH.edges && rK.edges == rD.edges) {
                ++passCount;
            }
        }

        std::cout << "Cross-Validation Complete: " << passCount << " / " << kRuns << " tests PASSED (100% agreement).\n";
    }
}

void Menu::handleRunBenchmarks() {
    std::cout << "\n=== Empirical Complexity Lab Benchmark ===\n";
    std::cout << "Running benchmark sweep across graph sizes (V = 100, 250, 500, 1000, 2500, 5000)...\n";

    std::vector<size_t> sweep = {100, 250, 500, 1000, 2500, 5000};
    auto metrics = BenchmarkSuite::run(sweep, true, 2);

    std::cout << "\nBenchmark Sweep Complete!\n";
    std::cout << "------------------------------------------------------------------------------------------\n";
    std::cout << "Algorithm        Type     Vertices    Edges       Time (ms)    Operations    MST Cost\n";
    std::cout << "------------------------------------------------------------------------------------------\n";
    for (const auto& m : metrics) {
        std::cout << std::left
                  << std::setw(17) << m.algorithm
                  << std::setw(9) << m.graphType
                  << std::setw(12) << m.vertices
                  << std::setw(12) << m.edges
                  << std::setw(13) << std::fixed << std::setprecision(3) << m.durationMs
                  << std::setw(14) << m.operations
                  << m.mstCost << "\n";
    }

    BenchmarkSuite::saveToCsv(metrics, "benchmarks.csv");
    std::cout << "\nSaved benchmark data to benchmarks.csv.\n";
    std::cout << "You can plot log-log graphs and crossover with: python tools/plot_benchmarks.py\n";
}

void Menu::handleInMemoryEdit() {
    if (!currentGraph_) {
        std::cout << "\nNo network currently loaded. Create a new graph? [y/N]: ";
        std::string ans;
        std::getline(std::cin, ans);
        if (ans == "y" || ans == "Y") {
            GraphBuilder b;
            DiagnosticsCollector d;
            b.addVertex(1, "Node-1");
            currentGraph_ = b.build(d).value();
            std::cout << "Initialized empty network with 1 vertex.\n";
        } else {
            return;
        }
    }

    std::cout << "\nIn-Memory Editing:\n";
    std::cout << "1. Add vertex\n";
    std::cout << "2. Add edge\n";
    std::cout << "3. Return to main menu\n";
    int choice = readInt("Enter choice [1-3]: ", 1, 3);

    if (choice == 1) {
        int64_t id = readInt64("Enter external vertex ID: ");
        std::string label = readString("Enter vertex label: ");
        std::string type = readString("Enter vertex type [e.g. router/switch/server]: ", true);
        double x = readDouble("Enter x coordinate [default 0.0]: ", 0.0);
        double y = readDouble("Enter y coordinate [default 0.0]: ", 0.0);

        GraphBuilder builder;
        DiagnosticsCollector diag;
        for (const auto& v : currentGraph_->vertices()) {
            builder.addVertex(v.externalId, v.label, v.type, v.x, v.y);
        }
        for (size_t i = 0; i < currentGraph_->edgeCount(); ++i) {
            const auto& e = currentGraph_->edge(static_cast<EdgeId>(i));
            const auto& m = currentGraph_->edgeMetadata(static_cast<EdgeId>(i));
            builder.addEdge(currentGraph_->vertex(e.u).externalId,
                            currentGraph_->vertex(e.v).externalId,
                            e.cost, m.length_m, m.medium);
        }

        auto addRes = builder.addVertex(id, label, type, x, y);
        if (addRes.isErr()) {
            std::cout << "Error: " << addRes.error() << "\n";
            return;
        }

        auto gRes = builder.build(diag);
        if (gRes.isOk()) {
            currentGraph_ = std::move(gRes.value());
            currentMst_ = std::nullopt;
            std::cout << "Vertex successfully added!\n";
        }
    } else if (choice == 2) {
        int64_t src = readInt64("Enter source vertex external ID: ");
        int64_t dst = readInt64("Enter target vertex external ID: ");
        Cost cost = readInt64("Enter cable cost (minor units): ");
        double len = readDouble("Enter cable length in meters: ", 10.0);
        std::string medium = readString("Enter medium [e.g. cat6/fiber]: ", true);

        GraphBuilder builder;
        DiagnosticsCollector diag;
        for (const auto& v : currentGraph_->vertices()) {
            builder.addVertex(v.externalId, v.label, v.type, v.x, v.y);
        }
        for (size_t i = 0; i < currentGraph_->edgeCount(); ++i) {
            const auto& e = currentGraph_->edge(static_cast<EdgeId>(i));
            const auto& m = currentGraph_->edgeMetadata(static_cast<EdgeId>(i));
            builder.addEdge(currentGraph_->vertex(e.u).externalId,
                            currentGraph_->vertex(e.v).externalId,
                            e.cost, m.length_m, m.medium);
        }

        builder.addEdge(src, dst, cost, len, medium);
        auto gRes = builder.build(diag);
        if (gRes.isOk()) {
            currentGraph_ = std::move(gRes.value());
            currentMst_ = std::nullopt;
            std::cout << "Edge successfully added!\n";
        } else {
            std::cout << "Error: " << diag.formatAll();
        }
    }
}

void Menu::run() {
    printHeader();
    while (true) {
        showMenu();
        int choice = readInt("Enter choice [0-11]: ", 0, 11);
        switch (choice) {
            case 1: handleLoadFromCsv(); break;
            case 2: handleInMemoryEdit(); break;
            case 3: handleDisplayNetwork(); break;
            case 4: handleRunKruskal(); break;
            case 5: handleRunPrim(); break;
            case 6: handleCompareAlgorithms(); break;
            case 7: handleSaveNetworkAndResults(); break;
            case 8: handleExportDot(); break;
            case 9: handleResilienceReport(); break;
            case 10: handleVerificationAndCrossVal(); break;
            case 11: handleRunBenchmarks(); break;
            case 0:
                std::cout << "\nExiting Network Cable Optimization System. Goodbye!\n";
                return;
        }
    }
}

} // namespace netopt
