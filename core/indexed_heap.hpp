#pragma once

#include "core/types.hpp"
#include <vector>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace netopt {

struct HeapNode {
    VertexId vertex{0};
    Cost key{std::numeric_limits<Cost>::max()};
    EdgeId bestEdgeId{std::numeric_limits<EdgeId>::max()};

    [[nodiscard]] constexpr bool operator<(const HeapNode& other) const noexcept {
        if (key != other.key) {
            return key < other.key;
        }
        if (bestEdgeId != other.bestEdgeId) {
            return bestEdgeId < other.bestEdgeId;
        }
        return vertex < other.vertex;
    }

    [[nodiscard]] constexpr bool operator>(const HeapNode& other) const noexcept {
        return other < *this;
    }
};

class Indexed4AryHeap {
public:
    static constexpr uint32_t kNotFound = std::numeric_limits<uint32_t>::max();

    explicit Indexed4AryHeap(size_t maxVertices = 0) {
        reset(maxVertices);
    }

    void reset(size_t maxVertices) {
        heap_.clear();
        pos_.assign(maxVertices, kNotFound);
        pushCount_ = 0;
        decreaseKeyCount_ = 0;
        popCount_ = 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return heap_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return heap_.size();
    }

    [[nodiscard]] bool contains(VertexId v) const noexcept {
        return v < pos_.size() && pos_[v] != kNotFound;
    }

    [[nodiscard]] Cost keyOf(VertexId v) const {
        if (!contains(v)) {
            throw std::out_of_range("Vertex not in heap");
        }
        return heap_[pos_[v]].key;
    }

    [[nodiscard]] EdgeId edgeOf(VertexId v) const {
        if (!contains(v)) {
            throw std::out_of_range("Vertex not in heap");
        }
        return heap_[pos_[v]].bestEdgeId;
    }

    void push(VertexId v, Cost key, EdgeId edgeId) {
        if (contains(v)) {
            decreaseKey(v, key, edgeId);
            return;
        }
        ++pushCount_;
        const uint32_t idx = static_cast<uint32_t>(heap_.size());
        heap_.push_back(HeapNode{v, key, edgeId});
        pos_[v] = idx;
        siftUp(idx);
    }

    bool decreaseKey(VertexId v, Cost newKey, EdgeId edgeId) {
        if (!contains(v)) {
            push(v, newKey, edgeId);
            return true;
        }
        const uint32_t idx = pos_[v];
        HeapNode candidate{v, newKey, edgeId};
        if (candidate < heap_[idx]) {
            ++decreaseKeyCount_;
            heap_[idx] = candidate;
            siftUp(idx);
            return true;
        }
        return false;
    }

    HeapNode pop() {
        if (heap_.empty()) {
            throw std::out_of_range("Pop from empty heap");
        }
        ++popCount_;
        HeapNode minNode = heap_.front();
        pos_[minNode.vertex] = kNotFound;

        const HeapNode lastNode = heap_.back();
        heap_.pop_back();

        if (!heap_.empty()) {
            heap_.front() = lastNode;
            pos_[lastNode.vertex] = 0;
            siftDown(0);
        }

        return minNode;
    }

    [[nodiscard]] uint64_t pushCount() const noexcept { return pushCount_; }
    [[nodiscard]] uint64_t decreaseKeyCount() const noexcept { return decreaseKeyCount_; }
    [[nodiscard]] uint64_t popCount() const noexcept { return popCount_; }

private:
    void siftUp(uint32_t idx) {
        while (idx > 0) {
            const uint32_t parent = (idx - 1) / 4;
            if (heap_[idx] < heap_[parent]) {
                swapNodes(idx, parent);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(uint32_t idx) {
        const size_t n = heap_.size();
        while (true) {
            const size_t firstChild = 4 * static_cast<size_t>(idx) + 1;
            if (firstChild >= n) {
                break;
            }

            size_t bestChild = firstChild;
            const size_t lastChild = std::min(firstChild + 4, n);
            for (size_t c = firstChild + 1; c < lastChild; ++c) {
                if (heap_[c] < heap_[bestChild]) {
                    bestChild = c;
                }
            }

            if (heap_[bestChild] < heap_[idx]) {
                swapNodes(idx, static_cast<uint32_t>(bestChild));
                idx = static_cast<uint32_t>(bestChild);
            } else {
                break;
            }
        }
    }

    void swapNodes(uint32_t i, uint32_t j) {
        std::swap(heap_[i], heap_[j]);
        pos_[heap_[i].vertex] = i;
        pos_[heap_[j].vertex] = j;
    }

    std::vector<HeapNode> heap_;
    std::vector<uint32_t> pos_;
    uint64_t pushCount_{0};
    uint64_t decreaseKeyCount_{0};
    uint64_t popCount_{0};
};

} // namespace netopt
