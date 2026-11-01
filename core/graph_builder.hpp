#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include "io/diagnostics.hpp"
#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

namespace netopt {

class GraphBuilder {
public:
    struct RawEdge {
        int64_t sourceExtId;
        int64_t targetExtId;
        Cost cost;
        double length_m{0.0};
        std::string medium;
        size_t sourceLine{0};
    };

    GraphBuilder() = default;

    Result<VertexId> addVertex(int64_t externalId,
                               std::string label = "",
                               std::string type = "",
                               double x = 0.0,
                               double y = 0.0,
                               size_t line = 0) {
        if (externalToDense_.contains(externalId)) {
            return Result<VertexId>::err("Duplicate vertex id: " + std::to_string(externalId));
        }

        const VertexId newId = static_cast<VertexId>(vertices_.size());
        externalToDense_[externalId] = newId;
        vertices_.push_back(Vertex{
            .externalId = externalId,
            .label = label.empty() ? ("V" + std::to_string(externalId)) : std::move(label),
            .type = std::move(type),
            .x = x,
            .y = y
        });
        vertexSourceLines_.push_back(line);
        return Result<VertexId>::ok(newId);
    }

    Result<void> addEdge(int64_t sourceExtId,
                         int64_t targetExtId,
                         Cost cost,
                         double length_m = 0.0,
                         std::string medium = "",
                         size_t line = 0) {
        rawEdges_.push_back(RawEdge{
            .sourceExtId = sourceExtId,
            .targetExtId = targetExtId,
            .cost = cost,
            .length_m = length_m,
            .medium = std::move(medium),
            .sourceLine = line
        });
        return Result<void>::ok();
    }

    [[nodiscard]] size_t vertexCount() const noexcept {
        return vertices_.size();
    }

    [[nodiscard]] size_t rawEdgeCount() const noexcept {
        return rawEdges_.size();
    }

    Result<Graph> build(DiagnosticsCollector& diagnostics, const std::string& filename = "") {
        if (vertices_.empty()) {
            diagnostics.addError(filename, 0, "No vertices defined in network");
            return Result<Graph>::err("Network has no vertices");
        }

        // Validate raw edges and check for unknown endpoints or negative costs
        bool hasFatalEdgeErrors = false;
        for (const auto& raw : rawEdges_) {
            if (!externalToDense_.contains(raw.sourceExtId)) {
                diagnostics.addError(filename, raw.sourceLine,
                    "Edge references unknown source vertex: " + std::to_string(raw.sourceExtId));
                hasFatalEdgeErrors = true;
            }
            if (!externalToDense_.contains(raw.targetExtId)) {
                diagnostics.addError(filename, raw.sourceLine,
                    "Edge references unknown target vertex: " + std::to_string(raw.targetExtId));
                hasFatalEdgeErrors = true;
            }
            if (raw.cost < 0) {
                diagnostics.addError(filename, raw.sourceLine,
                    "Negative edge cost (" + std::to_string(raw.cost) + ") is not allowed");
                hasFatalEdgeErrors = true;
            } else if (raw.cost == 0) {
                diagnostics.addWarning(filename, raw.sourceLine,
                    "Zero cost edge between " + std::to_string(raw.sourceExtId) +
                    " and " + std::to_string(raw.targetExtId) + " accepted");
            }
        }

        if (hasFatalEdgeErrors) {
            return Result<Graph>::err("Edge validation failed with errors");
        }

        // Filter self-loops and parallel/reversed edges
        // Map canonical undirected pair (min(u,v), max(u,v)) -> Best edge candidate
        struct Candidate {
            RawEdge edge;
            VertexId u;
            VertexId v;
            size_t originalIndex;
        };

        std::map<std::pair<VertexId, VertexId>, Candidate> bestCandidates;

        for (size_t i = 0; i < rawEdges_.size(); ++i) {
            const auto& raw = rawEdges_[i];
            const VertexId u = externalToDense_.at(raw.sourceExtId);
            const VertexId v = externalToDense_.at(raw.targetExtId);

            if (u == v) {
                diagnostics.addWarning(filename, raw.sourceLine,
                    "Self-loop on vertex " + std::to_string(raw.sourceExtId) + " dropped");
                continue;
            }

            const auto key = std::minmax(u, v);
            auto it = bestCandidates.find(key);
            if (it == bestCandidates.end()) {
                bestCandidates.emplace(key, Candidate{raw, u, v, i});
            } else {
                // Parallel or reversed duplicate
                if (raw.cost < it->second.edge.cost) {
                    diagnostics.addWarning(filename, it->second.edge.sourceLine,
                        "Parallel edge replaced by cheaper edge on line " + std::to_string(raw.sourceLine) +
                        " (previous cost: " + std::to_string(it->second.edge.cost) +
                        ", new cost: " + std::to_string(raw.cost) + ")");
                    it->second = Candidate{raw, u, v, i};
                } else {
                    diagnostics.addWarning(filename, raw.sourceLine,
                        "Parallel edge between " + std::to_string(raw.sourceExtId) + " and " +
                        std::to_string(raw.targetExtId) + " dropped (cost: " + std::to_string(raw.cost) +
                        " >= kept cost: " + std::to_string(it->second.edge.cost) + ")");
                }
            }
        }

        // Assemble the finalized Graph
        Graph g;
        g.vertices_ = vertices_;
        g.externalToDense_ = externalToDense_;

        const size_t numVertices = vertices_.size();
        std::vector<uint32_t> degree(numVertices, 0);

        // Sort accepted candidates by original index or deterministic order
        std::vector<Candidate> accepted;
        accepted.reserve(bestCandidates.size());
        for (auto& [_, cand] : bestCandidates) {
            accepted.push_back(cand);
        }
        std::sort(accepted.begin(), accepted.end(), [](const Candidate& a, const Candidate& b) {
            return a.originalIndex < b.originalIndex;
        });

        g.edges_.reserve(accepted.size());
        g.edgeMetadata_.reserve(accepted.size());

        for (size_t i = 0; i < accepted.size(); ++i) {
            const auto& cand = accepted[i];
            const EdgeId id = static_cast<EdgeId>(i);
            g.edges_.push_back(Edge{
                .u = cand.u,
                .v = cand.v,
                .cost = cand.edge.cost,
                .id = id
            });
            g.edgeMetadata_.push_back(EdgeMetadata{
                .length_m = cand.edge.length_m,
                .medium = cand.edge.medium
            });

            degree[cand.u]++;
            degree[cand.v]++;
        }

        // Build CSR offsets
        g.offsets_.resize(numVertices + 1, 0);
        g.offsets_[0] = 0;
        for (size_t i = 0; i < numVertices; ++i) {
            g.offsets_[i + 1] = g.offsets_[i] + degree[i];
        }

        g.arcs_.resize(g.offsets_.back());
        std::vector<uint32_t> currentOffset = g.offsets_;

        for (const auto& e : g.edges_) {
            g.arcs_[currentOffset[e.u]++] = Arc{.target = e.v, .cost = e.cost, .id = e.id};
            g.arcs_[currentOffset[e.v]++] = Arc{.target = e.u, .cost = e.cost, .id = e.id};
        }

        // Deterministically sort arcs incident to each vertex by (cost, id, target)
        for (VertexId u = 0; u < static_cast<VertexId>(numVertices); ++u) {
            const size_t start = g.offsets_[u];
            const size_t end = g.offsets_[u + 1];
            std::sort(g.arcs_.begin() + start, g.arcs_.begin() + end, [](const Arc& a, const Arc& b) {
                if (a.cost != b.cost) return a.cost < b.cost;
                if (a.id != b.id) return a.id < b.id;
                return a.target < b.target;
            });
        }

        return Result<Graph>::ok(std::move(g));
    }

private:
    std::vector<Vertex> vertices_;
    std::vector<size_t> vertexSourceLines_;
    std::unordered_map<int64_t, VertexId> externalToDense_;
    std::vector<RawEdge> rawEdges_;
};

} // namespace netopt
