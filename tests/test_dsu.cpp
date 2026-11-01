#include "doctest.h"
#include "core/dsu.hpp"

using namespace netopt;

TEST_CASE("DisjointSetUnion operations and invariants") {
    DisjointSet dsu(5);
    CHECK(dsu.components() == 5);
    for (VertexId i = 0; i < 5; ++i) {
        CHECK(dsu.find(i) == i);
        CHECK(dsu.size(i) == 1);
    }

    SUBCASE("Union of distinct elements") {
        CHECK(dsu.unite(0, 1));
        CHECK(dsu.components() == 4);
        CHECK(dsu.connected(0, 1));
        CHECK(dsu.size(0) == 2);
        CHECK(dsu.size(1) == 2);

        CHECK(dsu.unite(2, 3));
        CHECK(dsu.components() == 3);

        CHECK(dsu.unite(1, 3)); // merge {0,1} and {2,3}
        CHECK(dsu.components() == 2);
        CHECK(dsu.connected(0, 2));
        CHECK(dsu.connected(0, 3));
        CHECK(dsu.size(0) == 4);

        // Already united
        CHECK_FALSE(dsu.unite(0, 3));
        CHECK(dsu.components() == 2);
    }

    SUBCASE("Chain union and path halving") {
        DisjointSet chainDsu(100);
        for (VertexId i = 0; i < 99; ++i) {
            chainDsu.unite(i, i + 1);
        }
        CHECK(chainDsu.components() == 1);
        CHECK(chainDsu.size(0) == 100);
        for (VertexId i = 0; i < 100; ++i) {
            CHECK(chainDsu.connected(0, i));
        }
        CHECK(chainDsu.findCount() > 0);
    }
}
