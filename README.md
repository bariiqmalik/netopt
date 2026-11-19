# Network Cable Optimization System (`netopt`)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance, robust, menu-driven network cabling optimization system implemented in pure **C++20** using only standard library features.

`netopt` models computer networks as undirected, weighted graphs and computes optimal Minimum Spanning Trees (MST) and Spanning Forests using **Kruskal's algorithm**, **Prim's algorithm with an indexed 4-ary min-heap**, and **Prim's dense array $O(V^2)$ algorithm**.

---

## Key Highlights

- **Deterministic Total Ordering**: Strict composite ordering `(cost, edge_id)` guarantees **100% IDENTICAL edge sets** across Prim (Heap), Prim (Dense), and Kruskal on all graph topologies.
- **Cache-Conscious Data Structures**:
  - Compressed Sparse Row (CSR) adjacency representations (`offsets_` size $V+1$, `arcs_` size $2E$).
  - Indexed 4-ary Min-Heap with in-place $O(\log_4 V)$ `decreaseKey` and half the depth of binary heaps.
  - Disjoint Set Union (DSU) with iterative path halving and union-by-size ($O(\alpha(V))$ amortized).
- **Three Advanced Engineering Modules**:
  1. **Algorithm Replay Visualizer**: Writes frame-by-frame DOT traces and generates self-contained interactive HTML sliders (`tools/animate.py`).
  2. **Failure-Resilience & Sensitivity Engine**: Identifies critical network bridges (Single Points of Failure), calculates non-tree cable cost tolerances via $O(\log V)$ binary lifting, and computes the second-best MST.
  3. **Self-Verifying Engine & Benchmark Lab**: Validates 4 mathematical MST optimality invariants and sweeps graph scales ($V = 10^2 \dots 10^5$) with log-log scaling fits (`tools/plot_benchmarks.py`).
- **Resilient Multi-Diagnostic Parser**: Full RFC 4180 tokenizer supporting quotes, UTF-8 BOM, CRLF, and comprehensive line-numbered error reports.
- **Atomic File Persistence**: Saves files using `<filename>.tmp` before atomic `std::filesystem::rename` to prevent partial writes.

---

## Directory Architecture

```text
netopt/
├── CMakeLists.txt         # Root build configuration with sanitizer options
├── README.md              # Project documentation and build guide
├── core/                  # Core types, immutable CSR Graph, Builder, DSU, 4-Ary Heap
│   ├── types.hpp
│   ├── graph.hpp
│   ├── graph_builder.hpp
│   ├── dsu.hpp
│   └── indexed_heap.hpp
├── algo/                  # Strategy implementations of MST algorithms
│   ├── mst_algorithm.hpp
│   ├── mst_algorithm.cpp
│   ├── kruskal.cpp
│   ├── prim.cpp
│   └── prim_dense.cpp
├── io/                    # Robust CSV parser, validator, and atomic repository
│   ├── csv_tokenizer.hpp / .cpp
│   ├── row_validator.hpp / .cpp
│   ├── graph_repository.hpp / .cpp
│   └── diagnostics.hpp
├── analysis/              # Verification, sensitivity, and empirical benchmarks
│   ├── verifier.hpp / .cpp
│   ├── sensitivity.hpp / .cpp
│   └── benchmark.hpp / .cpp
├── export/                # Graphviz DOT exporter and event frame sink
│   ├── dot_exporter.hpp / .cpp
│   └── dot_frame_sink.hpp / .cpp
├── ui/                    # Robust interactive console menu
│   └── menu.hpp / .cpp
├── main.cpp               # Application entry point (interactive & headless)
├── tools/                 # Python animation and benchmark plotting utilities
│   ├── animate.py
│   ├── plot_benchmarks.py
│   └── make_report_assets.py
├── tests/                 # Unit, property, and 1,000+ randomized cross-validation tests
│   ├── doctest.h
│   ├── test_main.cpp
│   ├── test_core.cpp
│   ├── test_parser.cpp
│   ├── test_dsu.cpp
│   ├── test_heap.cpp
│   ├── test_kruskal.cpp
│   ├── test_prim.cpp
│   ├── test_verifier.cpp
│   ├── test_sensitivity.cpp
│   └── test_crossval.cpp
├── data/                  # Sample network fixtures
│   ├── sample_small_*
│   ├── sample_disconnected_*
│   ├── sample_invalid_*
│   └── sample_large_*
└── docs/                  # Technical documentation
    ├── ARCHITECTURE.md
    ├── DECISIONS.md
    ├── PSEUDOCODE.md
    ├── COMPLEXITY.md
    ├── TEST_CASES.md
    ├── USER_GUIDE.md
    └── results/           # Generated assets (transcripts, DOT, SVG charts, replay HTML)
```

---

## Build Instructions

### Prerequisites
- C++20 compliant compiler: GCC 11+, Clang 14+, or MSVC 2019+
- CMake 3.20+
- Ninja or Make

### Windows (GCC / MinGW / Clang / MSVC)
```powershell
# Using Ninja and GCC/Clang:
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Using Visual Studio (MSVC):
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Linux / macOS (GCC / Clang)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Sanitizer Build (AddressSanitizer + UndefinedBehaviorSanitizer)
```bash
cmake -B build_san -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build_san
```

---

## Running the System

### Interactive Console Menu
```bash
./build/netopt
```

### Direct Headless Evaluation
```bash
./build/netopt data/sample_small_vertices.csv data/sample_small_edges.csv
```

### Running Tests
Execute the 7,650-assertion test suite via `ctest`:
```bash
ctest --test-dir build --output-on-failure
```

### Generating Report Assets & Benchmark Plots
```bash
python tools/make_report_assets.py
```
This generates all transcripts, SVG plots, DOT diagrams, replay frames, and self-contained interactive replay HTML sliders (`docs/results/replay.html` and `docs/results/compare.html`).

---

## Algorithm Replay Visualizer (`tools/animate.py`)

`tools/animate.py` converts frame-by-frame DOT traces into **100% self-contained, interactive HTML players** with responsive inline SVG graphics (or base64-encoded PNGs). The generated HTML files work on any machine by direct double-click—**no web server, no CDN, and no external files required**.

### CLI Usage

```bash
# Single Algorithm Replay:
python tools/animate.py --frames <dir> --out replay.html [--format svg|png] [--title "..."]

# Side-by-Side Algorithm Comparison (e.g. Prim vs Kruskal):
python tools/animate.py --left <prim_dir> --right <kruskal_dir> --out compare.html

# Display Command-Line Help:
python tools/animate.py --help
```

### Graphviz Installation (Optional)

The system automatically detects `dot` in `PATH` or standard installation directories. To install Graphviz:
- **Windows**: `winget install Graphviz.Graphviz`
- **macOS**: `brew install graphviz`
- **Linux (Ubuntu/Debian)**: `sudo apt install graphviz`

### Graceful Fallback Mode

If Graphviz is not installed:
- `tools/animate.py` exits cleanly with code `0`.
- The HTML player is still generated with full interactive controls (Prev, Next, Play/Pause, range slider, keyboard navigation).
- A prominent yellow alert banner is displayed at the top explaining how to install Graphviz.
- The frame viewport displays syntax-highlighted DOT source code in place of rendered graphics.

### Interactive Controls & Keyboard Shortcuts

- **Prev / Next Buttons & Slider**: Step frame-by-frame or scrub through execution.
- **Play / Pause**: Auto-play algorithm progression at 1 frame per second.
- **Running Cost Badge**: Displays cumulative tree cost updated at each step.
- **Side-by-Side Mode**: Shows synchronized Prim vs Kruskal execution with automated MST cost match verification (`5,300` vs `5,300`).
- **Keyboard Shortcuts**:
  - `←` (Left Arrow): Previous step
  - `→` (Right Arrow): Next step
  - `Space`: Toggle Play / Pause
  - `Home`: Jump to first frame
  - `End`: Jump to final frame



---

## ⭐ Support & Star

If this implementation of Kruskal, Prim (4-ary heap), and CSR graphs in C++20 helped your understanding or coursework, **please consider giving this repository a star (⭐)!**

---

## 🔍 Explore More Projects by Bariq Malik

- 🎓 **[UniGrade Pro](https://github.com/bariiqmalik/unigrade-pro)** — Modern academic grade tracking, GPA/CGPA computation, and attendance predictor ([Live App](https://unigrade-pro.vercel.app)).
- 👗 **[Fitting Room AI](https://github.com/bariiqmalik/fitcheck)** — Personal AI stylist & smart digitized wardrobe with body shape calibration (React, Vite, Supabase).
- 🌾 **[Kisan Mandi Connect](https://github.com/bariiqmalik/kisan-mandi-connect)** — Indian agritech logistics solver using Dijkstra & 0/1 Knapsack DP in Core Java.
- 💼 **[Portfolio](https://github.com/bariiqmalik/portfolio)** — Developer portfolio ([Live Site](https://bariiqmalik.github.io/portfolio/)).