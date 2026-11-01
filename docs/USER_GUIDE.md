# User Guide: Network Cable Optimization System (`netopt`)

The **Network Cable Optimization System (`netopt`)** is a pure C++20 menu-driven console application designed to model telecommunications and computer networks, optimize physical cabling costs using Minimum Spanning Tree (MST) and Minimum Spanning Forest (MSF) algorithms, and perform sensitivity and resilience analysis.

---

## 1. Quick Start & Prerequisites

### 1.1. System Requirements
- **Operating System**: Windows (10/11), Linux (Ubuntu, Debian, Fedora, Arch), or macOS (12+).
- **C++ Compiler**: A standard C++20 compliant compiler:
  - GCC 11+
  - Clang 14+
  - MSVC 2019+ (v19.29+)
- **Build System**: CMake 3.20 or newer, and a build tool like Ninja or Make.
- **Optional Visualization**: Python 3.8+ (for replay HTML and benchmark plotting) and Graphviz (`dot` command) for rendering PNGs. `netopt` runs fully without Python or Graphviz installed, degrading gracefully.

### 1.2. Building the Project
From the root of the `netopt` repository:
```bash
# Configure with CMake and Ninja (or Make)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Compile all targets (netopt, netopt_tests, netopt_benchmark)
cmake --build build

# Run full automated test suite (7,650 assertions)
ctest --test-dir build --output-on-failure
```

---

## 2. Command-Line Usage

`netopt` supports both an interactive console menu and non-interactive batch modes:

### 2.1. Interactive Menu Mode (Default)
Simply run the executable without arguments:
```bash
./build/netopt
```

### 2.2. Headless Network Solver Mode
Process CSV files directly, compute the optimal network, verify it, and print a resilience analysis:
```bash
./build/netopt data/sample_small_vertices.csv data/sample_small_edges.csv
```

### 2.3. Algorithm Replay Mode
Generate step-by-step DOT animation frames for algorithm execution:
```bash
./build/netopt --replay data/sample_small_vertices.csv data/sample_small_edges.csv docs/results/replay_frames
```

### 2.4. DOT Export Mode
Export the computed MST directly to a Graphviz `.dot` file with critical bridges highlighted:
```bash
./build/netopt --export-dot data/sample_small_vertices.csv data/sample_small_edges.csv docs/results/mst.dot
```

---

## 3. CSV Data Schema Specification

Networks are specified using two UTF-8 CSV files: a vertices file and an edges file. The first line of each file must contain the schema comment `#netopt-schema:1`.

### 3.1. Vertices CSV (`vertices.csv`)
Header format:
```csv
#netopt-schema:1
id,label,type,x,y
1,Core-Router,router,10.0,20.0
2,Switch-Floor1,switch,25.0,20.0
3,Database-Server,server,40.0,15.0
```
- `id` (integer, required): Unique external identifier.
- `label` (string, optional): Human-readable name. Commas are permitted if wrapped in quotes (`"Room 101, Core"`).
- `type` (string, optional): Equipment classification (e.g. `router`, `switch`, `server`, `workstation`, `storage`).
- `x`, `y` (floats, optional): 2D spatial coordinates for mapping/layout.

### 3.2. Edges CSV (`edges.csv`)
Header format:
```csv
#netopt-schema:1
source,target,cost,length_m,medium
1,2,1200,30.0,cat6
2,3,2200,60.0,fiber
```
- `source`, `target` (integer, required): Endpoint vertex external IDs.
- `cost` (integer, required): Cable installation cost represented in **minor currency units** (e.g. cents, pence, or cents-equivalent). Floating point weights are strictly prohibited to avoid rounding errors.
- `length_m` (float, optional): Physical cable length in meters.
- `medium` (string, optional): Cabling specification (e.g. `cat6`, `fiber`, `dac`).

---

## 4. Interactive Menu Walkthrough

When launched, `netopt` presents an 11-option menu:

```text
---------------------------------- MAIN MENU -----------------------------------
 1. Load network from CSV files
 2. Add / remove vertex, add / remove edge (in-memory editing)
 3. Display current network topology
 4. Run Kruskal's algorithm
 5. Run Prim's algorithm (Heap / Dense / Auto)
 6. Compare Prim vs Kruskal (cost, edge set equality, runtime, operations)
 7. Save network and results (CSV / Text Report)
 8. Export Graphviz DOT (+ render PNG if Graphviz is installed)
 9. Failure-resilience & Sensitivity analysis (Bridges, Tolerances, 2nd-Best)
10. Run verification and cross-validation suite
11. Run empirical benchmark suite
 0. Exit
--------------------------------------------------------------------------------
```

### Option 1: Load Network from CSV
Prompts for paths to `vertices.csv` and `edges.csv`. The parser executes the complete tokenization, validation, and diagnostic pipeline. Any syntax warnings (such as dropped self-loops or parallel edges replaced by cheaper connections) are reported to the screen. If fatal errors exist, line-numbered diagnostics pinpoint the exact mistakes.

### Option 2: In-Memory Editing
Allows adding new network nodes or links dynamically without modifying the original source files on disk. Modified networks can subsequently be saved using Option 7.

### Option 3: Display Network Topology
Displays a tabular summary of the loaded network, including total vertex count, edge count, density, total original mesh cost, and the full list of vertices and connections.

### Options 4 & 5: Run Kruskal & Prim
Executes Kruskal's or Prim's algorithm (with a choice between Indexed 4-ary Heap, Dense $O(V^2)$, or Auto selection based on density threshold).
Results are displayed in a clean aligned table displaying:
- Chosen cable edges and their endpoints
- Per-edge costs
- Total optimized network cost
- Absolute budget savings and percentage saved compared to the full mesh
- Automatic verification against the 4 optimality properties (V-c edges, acyclicity, spanning, cycle property).

### Option 6: Compare Prim vs. Kruskal
Runs both Kruskal and Prim on the current topology side-by-side using high-resolution timers and operation counters (heap operations vs DSU finds). Confirms total cost equality and edge set identity.

### Option 7: Save Network and Results
Allows saving:
1. The currently loaded (or modified) network topology back to CSV.
2. The computed MST edges to `mst_result.csv`.
3. A comprehensive text report summarizing the network topology, chosen cables, and cost metrics.

### Option 8: Export Graphviz DOT
Exports the graph structure to Graphviz `.dot` format. If `dot` is installed on the host system, it automatically invokes `dot -Tpng` to render a high-resolution PNG diagram. If `dot` is missing, it notifies the user and preserves the `.dot` file without crashing.

### Option 9: Failure-Resilience & Sensitivity Analysis
Computes:
1. **Critical Bridges**: Tree edges that have no alternative replacement edge in the mesh. Removing a bridge permanently fragments the network.
2. **Protected Tree Edges**: Identifies the cheapest alternative replacement edge and the cost delta if that link fails.
3. **Non-Tree Tolerances**: For every unused cable, computes how much its cost must drop to displace an active cable in the MST.
4. **Second-Best MST**: Finds the lowest-cost alternative spanning tree.

### Option 10: Verification & Cross-Validation Suite
Allows running the self-verifying engine on the current MST or launching the 1,000+ randomized cross-validation suite comparing Prim (Heap), Prim (Dense), and Kruskal across random sparse, dense, disconnected, and ties-heavy graphs.

### Option 11: Run Empirical Benchmark Suite
Sweeps graph scales ($V = 100 \dots 5000$) across sparse and dense topologies, recording execution times and operation counts, and saves the results to `benchmarks.csv`.

---

## 5. Visualizations & Replay Tools

### 5.1. Algorithm Replay (HTML Slider / GIF)
Generate frame-by-frame DOT files during execution, then run:
```bash
python tools/animate.py --dir docs/results/replay_frames --output docs/results/replay.html
```
Open `docs/results/replay.html` in any web browser to interactively step forward, backward, or auto-play through the algorithm execution with color-coded edge events:
- **Green (Bold)**: Edge accepted into the tree.
- **Red (Dashed)**: Edge rejected (cycle detected).
- **Orange/Yellow**: Edge currently under candidate consideration.
- **Gray (Dotted)**: Unvisited potential cables.

### 5.2. Benchmark Plotting Tool
To visualize empirical complexity and scaling slopes:
```bash
python tools/plot_benchmarks.py --csv docs/results/benchmarks.csv --svg docs/results/benchmarks_plot.svg
```
Produces an SVG chart and reports the fitted scaling exponents (slopes $\beta$).
