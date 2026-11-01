#pragma once

#include "core/types.hpp"
#include <vector>
#include <cstdint>

namespace netopt {

class DisjointSet {
public:
    explicit DisjointSet(size_t n = 0) {
        reset(n);
    }

    void reset(size_t n) {
        // Root nodes hold negative size (-size). Initial size is 1, so -1.
        parentOrSize_.assign(n, -1);
        numComponents_ = n;
        findCount_ = 0;
    }

    // Iterative path halving for O(alpha(V)) amortized time without recursion
    VertexId find(VertexId x) {
        ++findCount_;
        while (parentOrSize_[x] >= 0) {
            const int32_t p = parentOrSize_[x];
            if (parentOrSize_[p] >= 0) {
                parentOrSize_[x] = parentOrSize_[p]; // path halving: make x point to its grandparent
                x = static_cast<VertexId>(parentOrSize_[p]);
            } else {
                x = static_cast<VertexId>(p);
                break;
            }
        }
        return x;
    }

    // Union by size
    bool unite(VertexId u, VertexId v) {
        VertexId rootU = find(u);
        VertexId rootV = find(v);
        if (rootU == rootV) {
            return false;
        }

        // parentOrSize_[root] is -size.
        // Make rootU the root with larger size (more negative value)
        if (parentOrSize_[rootU] > parentOrSize_[rootV]) {
            std::swap(rootU, rootV);
        }

        // rootU is larger, merge rootV into rootU
        parentOrSize_[rootU] += parentOrSize_[rootV];
        parentOrSize_[rootV] = static_cast<int32_t>(rootU);
        --numComponents_;
        return true;
    }

    [[nodiscard]] bool connected(VertexId u, VertexId v) {
        return find(u) == find(v);
    }

    [[nodiscard]] size_t components() const noexcept {
        return numComponents_;
    }

    [[nodiscard]] size_t size(VertexId u) {
        const VertexId root = find(u);
        return static_cast<size_t>(-parentOrSize_[root]);
    }

    [[nodiscard]] uint64_t findCount() const noexcept {
        return findCount_;
    }

private:
    std::vector<int32_t> parentOrSize_;
    size_t numComponents_{0};
    uint64_t findCount_{0};
};

} // namespace netopt
