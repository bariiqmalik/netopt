# Comprehensive Test Cases & Execution Verification (docs/TEST_CASES.md)

This document catalogs every automated test case implemented in `netopt_tests`, summarizing the input preconditions, expected outputs, verified actual outputs, and pass/fail statuses from the execution of the 7,650-assertion test suite.

---

## 1. Test Suite Summary Matrix

| Test Suite File | Test Cases | Total Assertions | Status | Coverage Focus |
|---|---|---|---|---|
| `tests/test_core.cpp` | 3 | 18 | **PASS** | `Result<T, E>` value semantics, `Edge` total order, `GraphBuilder` CSR construction |
| `tests/test_parser.cpp` | 2 | 26 | **PASS** | Edge-case table rows, BOM/CRLF/quotes, atomic save verification |
| `tests/test_dsu.cpp` | 1 | 18 | **PASS** | DSU union by size, path halving, components counter |
| `tests/test_heap.cpp` | 1 | 24 | **PASS** | Indexed 4-ary heap, `decreaseKey`, deterministic tie-breaking |
| `tests/test_kruskal.cpp` | 2 | 8 | **PASS** | CLRS textbook graph, disconnected spanning forest |
| `tests/test_prim.cpp` | 2 | 16 | **PASS** | Prim Heap & Dense textbook graph, disconnected forest, edge set equality |
| `tests/test_verifier.cpp` | 1 | 6 | **PASS** | 4 optimality properties, cycle rejection, non-tree path checking |
| `tests/test_sensitivity.cpp` | 1 | 10 | **PASS** | Bridge SPOF detection, edge tolerances, second-best MST |
| `tests/test_crossval.cpp` | 1 | 7,524 | **PASS** | 1,050 randomized graphs (sparse, dense, ties, disconnected) |
| **Total** | **14** | **7,650** | **100% PASS** | Zero failures, zero memory leaks, zero compiler warnings |

---

## 2. Detailed Test Case Catalog

### 2.1. Parser & Edge-Case Policy Table Tests

| Test ID | Case Description | Input Data / Precondition | Expected Output | Actual Output | Status |
|---|---|---|---|---|---|
| `TC-PARSER-01` | BOM & CRLF Tolerance | CSV starting with `\xEF\xBB\xBF` and `\r\n` line endings | BOM stripped, 4 rows parsed cleanly | 4 rows parsed, fields trimmed correctly | **PASS** |
| `TC-PARSER-02` | Quoted Fields & Commas | Field containing `"Room 101, Main"` and escaped `""` | Comma treated as literal, quote escaped | Field value: `Room 101, Main` | **PASS** |
| `TC-EDGE-01` | Empty / Header Only | Vertices CSV with header only: `#netopt-schema:1\nid,label,type,x,y\n` | Error: "No vertices defined (header only)" | Diagnostic Error emitted on line 2 | **PASS** |
| `TC-EDGE-02` | Missing / Extra Columns | Row has 4 columns instead of 5 (`1,V1,server,0.0`) | Error with line number 3 | Diagnostic: `v.csv:3: [ERROR] Wrong number of columns (expected 5, found 4)` | **PASS** |
| `TC-EDGE-03` | Non-Numeric / NaN Cost | Edges CSV row with cost = `NotANumber` | Error: non-numeric cost | Diagnostic: `e.csv:2: [ERROR] Non-numeric, NaN, or out-of-range edge cost` | **PASS** |
| `TC-EDGE-04` | Negative Edge Cost | Edge cost = `-500` | Error: negative cost disallowed | Diagnostic: `edges.csv:4: [ERROR] Negative edge cost (-500) is not allowed` | **PASS** |
| `TC-EDGE-05` | Zero Edge Cost | Edge cost = `0` | Warning logged, edge accepted into Graph | Diagnostic: `edges.csv:5: [WARNING] Zero cost edge accepted`, Graph has 1 edge | **PASS** |
| `TC-EDGE-06` | Duplicate Vertex ID | Two vertices with ID `1` | Error: duplicate vertex ID on line 4 | Diagnostic: `v.csv:4: [ERROR] Duplicate vertex id: 1` | **PASS** |
| `TC-EDGE-07` | Unknown Vertex Endpoint | Edge connects vertex `1` to non-existent `999` | Error: unknown target vertex | Diagnostic: `e.csv:10: [ERROR] Edge references unknown target vertex: 999` | **PASS** |
| `TC-EDGE-08` | Self-Loop | Edge connects vertex `1` to `1` | Warning logged, edge dropped | Diagnostic: `e.csv:12: [WARNING] Self-loop on vertex 1 dropped`, Graph edgeCount=0 | **PASS** |
| `TC-EDGE-09` | Parallel & Reversed Edges | Edge (1, 2) cost 500, edge (2, 1) cost 300 | Warning logged, cheapest kept (300) | Cheaper edge 300 kept, edgeCount=1 | **PASS** |
| `TC-EDGE-10` | Single Vertex, No Edges | Graph with 1 vertex, 0 edges | Valid Graph, MST cost = 0, components = 1 | `vertexCount=1, edgeCount=0, totalCost=0` | **PASS** |
| `TC-EDGE-11` | Disconnected Graph | 2 separate components and 1 isolated vertex | Spanning forest computed, 3 components | `components=3, edges=2, totalCost=40` | **PASS** |
| `TC-EDGE-12` | Missing File / Bad Path | Non-existent file path passed to repository | Error returned with clear message | `[ERROR] Failed to open file: non_existent_v.csv` | **PASS** |
| `TC-EDGE-13` | Atomic File Save | Save graph to `test_atomic_v.csv` | Writes to `.tmp`, renames atomically | File exists, `.tmp` cleanly replaced | **PASS** |

---

### 2.2. Algorithm & Data Structure Correctness Tests

| Test ID | Algorithm / Component | Fixture / Scenario | Expected Output | Actual Output | Status |
|---|---|---|---|---|---|
| `TC-DSU-01` | DisjointSet | 5 elements, unite disjoint pairs, chain test | 1 component, path halving verifies $O(\alpha(V))$ | `components=1, findCount > 0` | **PASS** |
| `TC-HEAP-01` | Indexed4AryHeap | Min-heap push, pop order, empty pop exception | Ascending order (10, 20, 50, 80) | Extracted in exact key order; empty pop throws | **PASS** |
| `TC-HEAP-02` | Indexed4AryHeap `decreaseKey` | Vertex 4 key decreased from 500 to 50 | Vertex 4 promoted to root and popped first | `top.vertex=4, top.key=50` | **PASS** |
| `TC-HEAP-03` | Deterministic Tie-Break | Two nodes with equal key 100, edge IDs 15 and 5 | Smaller edge ID 5 popped first | Node with edge ID 5 popped first | **PASS** |
| `TC-KRUSKAL-01`| Kruskal Algorithm | CLRS textbook 9-vertex graph | Total cost = 37, 8 edges, 1 component | `totalCost=37, edges.size()=8, components=1` | **PASS** |
| `TC-PRIM-01` | Prim (Heap) & Prim (Dense) | CLRS textbook 9-vertex graph | Total cost = 37, identical edge set to Kruskal | `totalCost=37, edges == rKruskal.edges` | **PASS** |
| `TC-PRIM-02` | Prim on Disconnected Graph | 3 components, disconnected campus network | Spanning forest with 3 components, cost = 40 | `components=3, totalCost=40, edges == Kruskal` | **PASS** |
| `TC-VERIF-01` | Verifier Engine | Optimal MST on triangle graph | Verified optimal | `valid=true, passed all 4 properties` | **PASS** |
| `TC-VERIF-02` | Verifier Engine | Suboptimal tree (cheaper edge omitted) | Fails cycle property | `valid=false, Cycle property violation` | **PASS** |
| `TC-VERIF-03` | Verifier Engine | Cyclic edge set | Fails edge count / acyclicity | `valid=false, Edge count mismatch` | **PASS** |
| `TC-SENS-01` | Sensitivity Analysis | Pendant branch to isolated leaf node | Flagged as critical bridge (SPOF) | `rep.criticalBridgeEdges.contains(bridgeId)` | **PASS** |
| `TC-SENS-02` | Sensitivity Analysis | Non-tree edge tolerance & second-best MST | Second best MST exists with cost delta $> 0$ | `secondBestMst.exists=true, delta > 0` | **PASS** |

---

### 2.3. Randomized Cross-Validation (1,050 Cases)

| Test ID | Seed | Iterations | Graph Topologies | Checked Invariants | Status |
|---|---|---|---|---|---|
| `TC-CROSSVAL-01` | `42133742` | **1,050** | Single-vertex, sparse ($p=0.15$), dense ($p=0.7$), complete ($p=0.95$), disconnected, ties-heavy ($w \in \{1, 2, 3, 4\}$) | 1. `rHeap.totalCost == rKruskal.totalCost`<br/>2. `rDense.totalCost == rKruskal.totalCost`<br/>3. `rHeap.components == rKruskal.components`<br/>4. `rDense.components == rKruskal.components`<br/>5. `rHeap.edges == rKruskal.edges` (100% Identical)<br/>6. `rDense.edges == rKruskal.edges` (100% Identical) | **100% PASS (1050/1050)** |
