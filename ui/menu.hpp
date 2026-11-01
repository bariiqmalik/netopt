#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include "core/graph_builder.hpp"
#include "algo/mst_algorithm.hpp"
#include "io/graph_repository.hpp"
#include <optional>
#include <string>

namespace netopt {

class Menu {
public:
    Menu();
    void run();

private:
    void printHeader() const;
    void showMenu() const;

    // Menu handlers
    void handleLoadFromCsv();
    void handleInMemoryEdit();
    void handleDisplayNetwork();
    void handleRunKruskal();
    void handleRunPrim();
    void handleCompareAlgorithms();
    void handleSaveNetworkAndResults();
    void handleExportDot();
    void handleResilienceReport();
    void handleVerificationAndCrossVal();
    void handleRunBenchmarks();

    // In-memory edit subroutines
    void handleAddVertex();
    void handleRemoveVertex();
    void handleAddEdge();
    void handleRemoveEdge();

    // Helper display
    void displayMstResult(const std::string& algoName, const MstResult& res);

    // Robust input helpers
    static int readInt(const std::string& prompt, int minVal, int maxVal);
    static int64_t readInt64(const std::string& prompt);
    static double readDouble(const std::string& prompt, double defaultVal = 0.0);
    static std::string readString(const std::string& prompt, bool allowEmpty = false);

    std::optional<Graph> currentGraph_;
    std::optional<MstResult> currentMst_;
    std::string lastAlgorithmRun_;
    std::string lastLoadedVerticesPath_;
    std::string lastLoadedEdgesPath_;
    CsvGraphRepository repo_;
};

} // namespace netopt
