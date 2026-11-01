#pragma once

#include "core/types.hpp"
#include <span>
#include <vector>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <numeric>

namespace netopt {

class GraphBuilder;

class Graph {
public:
    friend class GraphBuilder;

    Graph() = default;

    [[nodiscard]] size_t vertexCount() const noexcept {
        return vertices_.size();
    }

    [[nodiscard]] size_t edgeCount() const noexcept {
        return edges_.size();
    }

    [[nodiscard]] std::span<const Arc> neighbors(VertexId u) const noexcept {
        if (u >= vertices_.size() || offsets_.empty()) {
            return {};
        }
        const size_t start = offsets_[u];
        const size_t end = offsets_[u + 1];
        return std::span<const Arc>(arcs_.data() + start, end - start);
    }

    [[nodiscard]] const std::vector<Edge>& edges() const noexcept {
        return edges_;
    }

    [[nodiscard]] const Edge& edge(EdgeId id) const {
        if (id >= edges_.size()) {
            throw std::out_of_range("Invalid edge id");
        }
        return edges_[id];
    }

    [[nodiscard]] const Vertex& vertex(VertexId u) const {
        if (u >= vertices_.size()) {
            throw std::out_of_range("Invalid vertex id");
        }
        return vertices_[u];
    }

    [[nodiscard]] const std::vector<Vertex>& vertices() const noexcept {
        return vertices_;
    }

    [[nodiscard]] std::optional<VertexId> findDenseId(int64_t externalId) const noexcept {
        auto it = externalToDense_.find(externalId);
        if (it != externalToDense_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] const EdgeMetadata& edgeMetadata(EdgeId id) const {
        if (id >= edgeMetadata_.size()) {
            throw std::out_of_range("Invalid edge metadata id");
        }
        return edgeMetadata_[id];
    }

    [[nodiscard]] Cost totalOriginalCost() const noexcept {
        Cost total = 0;
        for (const auto& e : edges_) {
            total += e.cost;
        }
        return total;
    }

    [[nodiscard]] double density() const noexcept {
        const size_t v = vertexCount();
        if (v < 2) return 0.0;
        const double maxEdges = static_cast<double>(v) * static_cast<double>(v - 1) / 2.0;
        return static_cast<double>(edgeCount()) / maxEdges;
    }

private:
    std::vector<Vertex> vertices_;
    std::unordered_map<int64_t, VertexId> externalToDense_;
    std::vector<Edge> edges_;
    std::vector<EdgeMetadata> edgeMetadata_;
    std::vector<uint32_t> offsets_;
    std::vector<Arc> arcs_;
};

} // namespace netopt
