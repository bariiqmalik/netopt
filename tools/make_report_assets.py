#!/usr/bin/env python3
"""
Automated Report Asset Generation Script for netopt
Runs sample datasets through netopt, records console transcripts,
exports DOT files, generates algorithm replay frames and HTML slider,
and compiles benchmark charts for technical reports.
"""

import os
import sys
import subprocess
import glob

def run():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    os.chdir(root_dir)

    results_dir = os.path.join(root_dir, "docs", "results")
    os.makedirs(results_dir, exist_ok=True)
    replay_dir = os.path.join(results_dir, "replay_frames")
    os.makedirs(replay_dir, exist_ok=True)

    exe_path = os.path.join(root_dir, "build", "netopt.exe")
    if not os.path.exists(exe_path):
        print("netopt.exe not found in build/ directory.")
        return

    # 1. Generate console transcripts for sample datasets
    datasets = [
        ("small", "data/sample_small_vertices.csv", "data/sample_small_edges.csv"),
        ("disconnected", "data/sample_disconnected_vertices.csv", "data/sample_disconnected_edges.csv"),
        ("large", "data/sample_large_vertices.csv", "data/sample_large_edges.csv")
    ]

    for name, v_csv, e_csv in datasets:
        print(f"Generating transcript for sample_{name}...")
        res = subprocess.run([exe_path, v_csv, e_csv], capture_output=True, text=True)
        txt_out = os.path.join(results_dir, f"transcript_{name}.txt")
        with open(txt_out, "w", encoding="utf-8") as f:
            f.write(res.stdout)
            if res.stderr:
                f.write("\nSTDERR:\n" + res.stderr)
        print(f"Saved {txt_out}")

    # 2. Run benchmarks and generate charts
    bench_exe = os.path.join(root_dir, "build", "netopt_benchmark.exe")
    if os.path.exists(bench_exe):
        print("Running benchmark suite...")
        subprocess.run([bench_exe, "--quick"], check=True)
        print("Plotting benchmark graphs...")
        plot_script = os.path.join(root_dir, "tools", "plot_benchmarks.py")
        subprocess.run([sys.executable, plot_script, "--csv", "docs/results/benchmarks.csv"], check=True)

    # 3. Generate replay frames and export DOT files
    print("Generating replay frames for sample_small...")
    subprocess.run([exe_path, "--replay", "data/sample_small_vertices.csv", "data/sample_small_edges.csv", replay_dir], check=True)

    print("Exporting DOT files...")
    subprocess.run([exe_path, "--export-dot", "data/sample_small_vertices.csv", "data/sample_small_edges.csv", os.path.join(results_dir, "small_mst.dot")], check=True)
    subprocess.run([exe_path, "--export-dot", "data/sample_disconnected_vertices.csv", "data/sample_disconnected_edges.csv", os.path.join(results_dir, "disconnected_mst.dot")], check=True)
    subprocess.run([exe_path, "--export-dot", "data/sample_large_vertices.csv", "data/sample_large_edges.csv", os.path.join(results_dir, "large_mst.dot")], check=True)

    # 4. Create interactive replay player and side-by-side comparison using animate.py
    animate_script = os.path.join(root_dir, "tools", "animate.py")
    if os.path.exists(animate_script):
        print("Generating animation replay slider...")
        subprocess.run([sys.executable, animate_script, "--frames", replay_dir, "--out", os.path.join(results_dir, "replay.html")], check=True)

        prim_replay_dir = os.path.join(results_dir, "replay_prim")
        kruskal_replay_dir = os.path.join(results_dir, "replay_kruskal")
        compare_html = os.path.join(results_dir, "compare.html")
        print("Generating side-by-side comparison HTML...")
        subprocess.run([
            exe_path,
            "--compare-replay",
            "data/sample_small_vertices.csv",
            "data/sample_small_edges.csv",
            prim_replay_dir,
            kruskal_replay_dir,
            compare_html
        ], check=True)

    print("\nAll report assets successfully generated in docs/results/!")

if __name__ == "__main__":
    run()
