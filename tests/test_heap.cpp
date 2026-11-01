#include "doctest.h"
#include "core/indexed_heap.hpp"

using namespace netopt;

TEST_CASE("Indexed4AryHeap operations and decreaseKey") {
    Indexed4AryHeap heap(10);
    CHECK(heap.empty());
    CHECK(heap.size() == 0);

    SUBCASE("Push, contains, and min-order extraction") {
        heap.push(1, 50, 101);
        heap.push(2, 20, 102);
        heap.push(3, 80, 103);
        heap.push(4, 10, 104);

        CHECK(heap.size() == 4);
        CHECK(heap.contains(2));
        CHECK(heap.keyOf(2) == 20);
        CHECK(heap.edgeOf(2) == 102);

        // Pop in ascending key order
        auto n1 = heap.pop();
        CHECK(n1.vertex == 4);
        CHECK(n1.key == 10);
        CHECK(n1.bestEdgeId == 104);

        auto n2 = heap.pop();
        CHECK(n2.vertex == 2);
        CHECK(n2.key == 20);

        auto n3 = heap.pop();
        CHECK(n3.vertex == 1);
        CHECK(n3.key == 50);

        auto n4 = heap.pop();
        CHECK(n4.vertex == 3);
        CHECK(n4.key == 80);

        CHECK(heap.empty());
        CHECK_THROWS_AS(heap.pop(), std::out_of_range);
    }

    SUBCASE("decreaseKey updates key and restores heap property") {
        heap.push(0, 100, 1);
        heap.push(1, 200, 2);
        heap.push(2, 300, 3);
        heap.push(3, 400, 4);
        heap.push(4, 500, 5);

        // Decrease vertex 4 from 500 to 50
        CHECK(heap.decreaseKey(4, 50, 50));
        CHECK(heap.keyOf(4) == 50);

        // Should now pop vertex 4 first
        auto top = heap.pop();
        CHECK(top.vertex == 4);
        CHECK(top.key == 50);

        // Attempting to increase key should return false and not change key
        CHECK_FALSE(heap.decreaseKey(0, 150, 10));
        CHECK(heap.keyOf(0) == 100);
    }

    SUBCASE("Deterministic tie-breaking on equal cost") {
        // Equal cost, different edge IDs
        heap.push(1, 100, 15);
        heap.push(2, 100, 5); // smaller edge ID 5

        auto top = heap.pop();
        CHECK(top.vertex == 2);
        CHECK(top.bestEdgeId == 5);

        auto next = heap.pop();
        CHECK(next.vertex == 1);
        CHECK(next.bestEdgeId == 15);
    }
}
