# Architectural and Design Decisions (docs/DECISIONS.md)

This log documents all architectural decisions, design choices, and tradeoffs made during the design and implementation of the **Network Cable Optimization System (`netopt`)**.

---

## Decision 1: Deterministic Tie-Breaking for Prim, PrimDense, and Kruskal
* **Context**: Graphs with duplicate edge weights can yield multiple valid Minimum Spanning Trees. Acceptance criteria require that Prim (heap), PrimDense, and Kruskal produce **IDENTICAL** edge sets across all graphs, including 1000+ randomized test fixtures.
* **Decision**: Define a strict total ordering for all edges using a composite key: `(cost, id)`.
  - In Kruskal: Edges are sorted by `(edge.cost, edge.id)`.
  - In Prim (Heap): The priority key for candidate vertices is `(min_incident_cost, min_incident_edge_id)`.
  - In PrimDense: Linear scans select the unvisited vertex minimizing `(dist[v], bestEdgeId[v])`.
* **Consequence**: Guaranteed deterministic edge selection across all MST algorithms regardless of vertex traversal order or density mode.

---

## Decision 2: Cost Representation as Integral Minor Currency Units (`int64_t`)
* **Context**: Network cabling budgets require high precision without rounding anomalies or non-associative arithmetic.
* **Decision**: Use `int64_t` for all cable costs, representing minor currency units (e.g., cents, pennies, or cents-equivalent).
* **Consequence**: Avoids floating point inaccuracies, `NaN`, `-0.0`, and epsilon-comparison issues. Fully satisfies the strict integer weight constraint.

---

## Decision 3: Compressed Sparse Row (CSR) + Edge List Representation
* **Context**: Graph operations need both efficient neighborhood iteration (Prim, verifier DFS, cycle checks) and flat edge sequence iteration (Kruskal, CSV serialization).
* **Decision**: `Graph` stores:
  1. A CSR structure (`offsets_` of size $V+1$, `arcs_` of size $2E$ for undirected edges).
  2. A contiguous `edges_` vector of size $E$ storing `{u, v, cost, id}`.
* **Consequence**: Optimal $O(1)$ vertex degree lookup, contiguous cache lines for neighborhood scans, and zero per-vertex heap allocation overhead.

---

## Decision 4: Selection of Indexed 4-Ary Heap Over Binary Heap and Fibonacci Heap
* **Context**: Prim's algorithm on sparse graphs requires an efficient priority queue with `decreaseKey`.
* **Decision**: Use an indexed 4-ary min-heap with an auxiliary `pos[]` array mapping `VertexId -> index in heap`.
* **Justification**:
  - **4-ary vs Binary**: Reduces tree depth from $\log_2 V$ to $\log_4 V$ (a 50% decrease in heap height). Four children fit neatly into modern 64-byte cache lines, significantly improving memory access locality during sift-down.
  - **Rejection of Fibonacci Heap**: While Fibonacci heaps theoretically offer $O(1)$ amortized `decreaseKey`, their empirical performance is plagued by high constant factors, pointer chasing across doubly-linked circular lists, dynamic node allocation, and poor spatial locality.
* **Consequence**: Cache-optimal, predictable, low-constant-factor $O(\log_4 V)$ operations with no dynamic allocation during algorithm execution.

---

## Decision 5: Disjoint Set Union (DSU) with Path Halving and Union by Size
* **Context**: Kruskal's algorithm requires dynamic connectivity queries and union operations.
* **Decision**: Implement DSU using a single `std::vector<int32_t>` where root vertices store `-size` and non-roots store parent pointers. Path compression is done via single-pass iterative path halving (`parent[x] = parent[parent[x]]`).
* **Consequence**: Zero recursion overhead (eliminating risk of stack overflow on deep linear chains) and optimal $O(\alpha(V))$ amortized complexity per operation.

---

## Decision 6: Error Handling via Custom `Result<T, Error>`
* **Context**: C++23 features like `std::expected` are strictly prohibited by the specification; raw exceptions are suboptimal for high-frequency CSV parsing and validation where multiple errors should be accumulated.
* **Decision**: Implement a lightweight, value-based `Result<T, E>` template in `core/types.hpp`.
* **Consequence**: Zero-cost, standard C++20 compliant error handling with explicit propagation and no hidden runtime cost.

---

## Decision 7: Multi-Diagnostic Accumulation in Parser
* **Context**: CSV files may contain multiple syntax or semantic errors across different rows.
* **Decision**: The parser does not fail on the first error; instead, `DiagnosticsCollector` collects all warnings and errors paired with file name and line number, returning a comprehensive diagnostic report.
* **Consequence**: Superior user experience when debugging faulty CSV data; directly satisfies the acceptance criteria.

---

## Decision 8: Atomic File Write Strategy
* **Context**: Corrupting network topology files during saving (due to system crash or partial writes) is unacceptable.
* **Decision**: All file write operations write completely to a temporary file `<filename>.tmp` and then call `std::filesystem::rename` to atomically replace the target file.
* **Consequence**: Guarantees file integrity under all normal operating conditions.

---

## Decision 9: Sensitivity and Second-Best MST Computation via Binary Lifting
* **Context**: Finding the non-tree edge tolerance requires determining the maximum edge on the unique cycle formed by adding a non-tree edge $(u, v)$ to the MST.
* **Decision**: Implement $O(V \log V)$ tree preprocessing with binary lifting (`up[k][v]` and `maxEdge[k][v]`), enabling $O(\log V)$ queries for path-maximum edge per non-tree edge.
* **Consequence**: Computes all edge tolerances, bridge detection, and second-best MST in $O(E \log V)$ total time, scaling efficiently even to large graphs.
