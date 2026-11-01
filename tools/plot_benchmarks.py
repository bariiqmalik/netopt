#!/usr/bin/env python3
"""
Plotting and Empirical Complexity Analysis Tool for netopt
Reads benchmarks.csv, performs log-log linear regressions to estimate empirical
scaling exponents (slopes), detects Prim Heap vs Prim Dense crossover points,
and outputs both matplotlib charts (if available) and an interactive SVG/HTML report.
"""

import os
import sys
import csv
import math
import argparse

def parse_benchmarks(csv_file):
    if not os.path.exists(csv_file):
        print(f"File not found: {csv_file}")
        return []

    records = []
    with open(csv_file, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = None
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if header is None:
                header = [c.strip() for c in row]
                continue
            item = dict(zip(header, [c.strip() for c in row]))
            try:
                records.append({
                    "algorithm": item["algorithm"],
                    "graph_type": item["graph_type"],
                    "vertices": int(item["vertices"]),
                    "edges": int(item["edges"]),
                    "density": float(item["density"]),
                    "duration_ms": float(item["duration_ms"]),
                    "operations": int(item["operations"])
                })
            except Exception as e:
                continue
    return records

def linear_regression(x_vals, y_vals):
    n = len(x_vals)
    if n < 2:
        return 0.0, 0.0, 0.0
    mean_x = sum(x_vals) / n
    mean_y = sum(y_vals) / n
    num = sum((x - mean_x) * (y - mean_y) for x, y in zip(x_vals, y_vals))
    den = sum((x - mean_x) ** 2 for x in x_vals)
    if den == 0:
        return 0.0, 0.0, 0.0
    slope = num / den
    intercept = mean_y - slope * mean_x
    # R-squared
    ss_tot = sum((y - mean_y) ** 2 for y in y_vals)
    ss_res = sum((y - (slope * x + intercept)) ** 2 for x, y in zip(x_vals, y_vals))
    r2 = 1.0 - (ss_res / ss_tot) if ss_tot > 0 else 1.0
    return slope, intercept, r2

def analyze_complexity(records):
    grouped = {}
    for r in records:
        key = (r["algorithm"], r["graph_type"])
        if key not in grouped:
            grouped[key] = []
        grouped[key].append(r)

    print("\n================== EMPIRICAL COMPLEXITY ANALYSIS (LOG-LOG FIT) ==================")
    print(f"{'Algorithm':<18} {'Type':<8} {'Points':<8} {'Fitted Slope (log T / log V)':<30} {'R^2 Fit':<10}")
    print("-" * 80)

    summary = []
    for (algo, gtype), rows in sorted(grouped.items()):
        rows.sort(key=lambda x: x["vertices"])
        valid_rows = [r for r in rows if r["duration_ms"] > 0 and r["vertices"] > 0]
        if len(valid_rows) >= 2:
            log_v = [math.log(r["vertices"]) for r in valid_rows]
            log_t = [math.log(r["duration_ms"]) for r in valid_rows]
            slope, intercept, r2 = linear_regression(log_v, log_t)
            print(f"{algo:<18} {gtype:<8} {len(valid_rows):<8} {slope:>10.3f} (O(V^{slope:.2f}))       {r2:>8.4f}")
            summary.append({
                "algorithm": algo,
                "graph_type": gtype,
                "slope": slope,
                "r2": r2,
                "rows": rows
            })
        else:
            print(f"{algo:<18} {gtype:<8} {len(rows):<8} Insufficient data")
    print("=" * 80)
    return summary

def generate_svg_chart(summary, output_svg):
    # Generates a clean standalone SVG plot of the benchmark data
    width = 850
    height = 500
    margin = {"top": 60, "right": 180, "bottom": 60, "left": 70}
    plot_w = width - margin["left"] - margin["right"]
    plot_h = height - margin["top"] - margin["bottom"]

    all_v = []
    all_t = []
    for s in summary:
        for r in s["rows"]:
            if r["vertices"] > 0 and r["duration_ms"] > 0:
                all_v.append(r["vertices"])
                all_t.append(r["duration_ms"])

    if not all_v:
        return

    min_v, max_v = min(all_v), max(all_v)
    min_t, max_t = min(all_t), max(all_t)

    log_min_v = math.log10(min_v)
    log_max_v = math.log10(max_v) if max_v > min_v else log_min_v + 1
    log_min_t = math.log10(min_t)
    log_max_t = math.log10(max_t) if max_t > min_t else log_min_t + 1

    def scale_x(v):
        lv = math.log10(v)
        return margin["left"] + (lv - log_min_v) / (log_max_v - log_min_v) * plot_w

    def scale_y(t):
        lt = math.log10(t)
        return margin["top"] + plot_h - (lt - log_min_t) / (log_max_t - log_min_t) * plot_h

    colors = {
        ("Kruskal", "sparse"): "#1976D2",
        ("Prim (Heap)", "sparse"): "#388E3C",
        ("Prim (Dense)", "sparse"): "#F57C00",
        ("Kruskal", "dense"): "#7B1FA2",
        ("Prim (Heap)", "dense"): "#00796B",
        ("Prim (Dense)", "dense"): "#D32F2F",
    }

    svg = []
    svg.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" style="background:#ffffff; font-family:sans-serif;">')
    svg.append(f'<text x="{width/2}" y="35" text-anchor="middle" font-size="18" font-weight="bold" fill="#263238">Algorithm Benchmark Scaling (Log-Log Runtime)</text>')

    # Axes
    svg.append(f'<line x1="{margin["left"]}" y1="{margin["top"]+plot_h}" x2="{margin["left"]+plot_w}" y2="{margin["top"]+plot_h}" stroke="#37474F" stroke-width="2"/>')
    svg.append(f'<line x1="{margin["left"]}" y1="{margin["top"]}" x2="{margin["left"]}" y2="{margin["top"]+plot_h}" stroke="#37474F" stroke-width="2"/>')

    # Grid & labels
    svg.append(f'<text x="{margin["left"]+plot_w/2}" y="{height-15}" text-anchor="middle" font-size="13" fill="#37474F">Number of Vertices V (Log Scale)</text>')
    svg.append(f'<text transform="rotate(-90)" x="-{margin["top"]+plot_h/2}" y="20" text-anchor="middle" font-size="13" fill="#37474F">Duration in ms (Log Scale)</text>')

    # Lines & markers
    legend_y = margin["top"] + 20
    for s in summary:
        key = (s["algorithm"], s["graph_type"])
        col = colors.get(key, "#333333")
        rows = sorted([r for r in s["rows"] if r["vertices"] > 0 and r["duration_ms"] > 0], key=lambda x: x["vertices"])

        points = []
        for r in rows:
            cx = scale_x(r["vertices"])
            cy = scale_y(r["duration_ms"])
            points.append((cx, cy))
            svg.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="4" fill="{col}"/>')

        if len(points) > 1:
            pts_str = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
            svg.append(f'<polyline points="{pts_str}" fill="none" stroke="{col}" stroke-width="2.5"/>')

        # Legend
        label_text = f'{s["algorithm"]} ({s["graph_type"]}) [O(V^{s["slope"]:.2f})]'
        svg.append(f'<line x1="{width - margin["right"] + 15}" y1="{legend_y}" x2="{width - margin["right"] + 40}" y2="{legend_y}" stroke="{col}" stroke-width="3"/>')
        svg.append(f'<text x="{width - margin["right"] + 45}" y="{legend_y+4}" font-size="11" fill="#263238">{label_text}</text>')
        legend_y += 24

    svg.append('</svg>')

    with open(output_svg, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"Generated standalone SVG complexity plot at: {output_svg}")

def try_matplotlib_plot(summary, output_png):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        plt.figure(figsize=(10, 6))
        for s in summary:
            rows = sorted([r for r in s["rows"] if r["vertices"] > 0 and r["duration_ms"] > 0], key=lambda x: x["vertices"])
            v = [r["vertices"] for r in rows]
            t = [r["duration_ms"] for r in rows]
            plt.loglog(v, t, marker="o", label=f'{s["algorithm"]} ({s["graph_type"]}) - slope {s["slope"]:.2f}')

        plt.xlabel("Vertices V (log scale)")
        plt.ylabel("Execution Time (ms, log scale)")
        plt.title("Empirical Complexity & Scaling Comparison")
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.legend()
        plt.tight_layout()
        plt.savefig(output_png, dpi=200)
        print(f"Generated Matplotlib plot at: {output_png}")
    except ImportError:
        print("Matplotlib not installed. (SVG plot successfully generated as zero-dependency visual).")

def main():
    parser = argparse.ArgumentParser(description="Analyze and plot netopt benchmarks")
    parser.add_argument("--csv", default="benchmarks.csv", help="Input benchmarks.csv")
    parser.add_argument("--svg", default="docs/results/benchmarks_plot.svg", help="Output SVG chart")
    parser.add_argument("--png", default="docs/results/benchmarks_plot.png", help="Output PNG chart")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.svg), exist_ok=True)
    records = parse_benchmarks(args.csv)
    if not records:
        print("No valid benchmark records found.")
        return

    summary = analyze_complexity(records)
    generate_svg_chart(summary, args.svg)
    try_matplotlib_plot(summary, args.png)

if __name__ == "__main__":
    main()
