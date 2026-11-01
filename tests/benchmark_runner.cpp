#include "analysis/benchmark.hpp"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::cout << "=== Network Cable Optimization System - Benchmark Lab ===\n";
    std::vector<size_t> sweep;

    if (argc > 1 && std::string(argv[1]) == "--quick") {
        std::cout << "Running quick sweep for testing...\n";
        sweep = {50, 100, 200, 400};
    } else {
        std::cout << "Running full empirical complexity sweep (V = 100 .. 20000)...\n";
        sweep = {100, 250, 500, 1000, 2500, 5000, 10000};
    }

    auto metrics = netopt::BenchmarkSuite::run(sweep, true, 2);

    std::cout << "\nBenchmark Results:\n";
    std::cout << "------------------------------------------------------------------------------------------\n";
    std::cout << "Algorithm        Type     Vertices    Edges       Time (ms)    Operations    MST Cost\n";
    std::cout << "------------------------------------------------------------------------------------------\n";
    for (const auto& m : metrics) {
        std::cout << std::left
                  << std::setw(17) << m.algorithm
                  << std::setw(9) << m.graphType
                  << std::setw(12) << m.vertices
                  << std::setw(12) << m.edges
                  << std::setw(13) << m.durationMs
                  << std::setw(14) << m.operations
                  << m.mstCost << "\n";
    }

    std::filesystem::create_directories("docs/results");
    netopt::BenchmarkSuite::saveToCsv(metrics, "benchmarks.csv");
    netopt::BenchmarkSuite::saveToCsv(metrics, "docs/results/benchmarks.csv");
    std::cout << "\nWrote benchmarks to benchmarks.csv and docs/results/benchmarks.csv\n";
    return 0;
}
