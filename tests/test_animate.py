#!/usr/bin/env python3
"""
Automated Unit and Integration Tests for tools/animate.py
Tests single replay, Graphviz SVG rendering, fallback mode when dot is absent,
empty frames error handling, and side-by-side Prim vs Kruskal comparison.
"""

import os
import sys
import shutil
import tempfile
import unittest
import subprocess

# Ensure repo root is on sys.path
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.animate import find_dot

class TestAnimate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.python_exe = sys.executable
        cls.animate_script = os.path.join(REPO_ROOT, "tools", "animate.py")
        cls.sample_v = os.path.join(REPO_ROOT, "data", "sample_small_vertices.csv")
        cls.sample_e = os.path.join(REPO_ROOT, "data", "sample_small_edges.csv")

        # Create temporary working directory for test outputs
        cls.test_dir = tempfile.mkdtemp(prefix="netopt_anim_test_")

        # Create fixture frames
        cls.fixtures_dir = os.path.join(cls.test_dir, "fixtures")
        os.makedirs(cls.fixtures_dir, exist_ok=True)

        for i in range(1, 4):
            dot_content = f"""graph Frame {{
  label="Test Step {i}\\nRunning Cost: {i * 100} | Step {i}";
  labelloc="t";
  node [shape=box];
  1 [label="NodeA"];
  2 [label="NodeB"];
  1 -- 2 [label="100", color="#2E7D32"];
}}
"""
            with open(os.path.join(cls.fixtures_dir, f"frame_{i:03d}.dot"), "w", encoding="utf-8") as f:
                f.write(dot_content)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.test_dir, ignore_errors=True)

    def test_empty_frames_directory_fails_with_nonzero_exit(self):
        """Empty directory must produce clear error and non-zero exit code."""
        empty_dir = os.path.join(self.test_dir, "empty_dir")
        os.makedirs(empty_dir, exist_ok=True)
        out_html = os.path.join(self.test_dir, "empty_out.html")

        res = subprocess.run(
            [self.python_exe, self.animate_script, "--frames", empty_dir, "--out", out_html],
            capture_output=True,
            text=True
        )

        self.assertNotEqual(res.returncode, 0, "Expected non-zero exit on empty frames directory")
        self.assertIn("No .dot frames found", res.stderr)
        self.assertFalse(os.path.exists(out_html))

    def test_missing_dot_fallback_mode(self):
        """When dot is missing/hidden, HTML slider still works with yellow banner and exit 0."""
        out_html = os.path.join(self.test_dir, "fallback_out.html")
        env = os.environ.copy()
        env["NETOPT_SIMULATE_NO_DOT"] = "1"

        res = subprocess.run(
            [self.python_exe, self.animate_script, "--frames", self.fixtures_dir, "--out", out_html],
            capture_output=True,
            text=True,
            env=env
        )

        self.assertEqual(res.returncode, 0, f"Expected 0 exit in fallback mode, got: {res.stderr}")
        self.assertTrue(os.path.exists(out_html), "Expected HTML file to be generated")

        with open(out_html, "r", encoding="utf-8") as f:
            content = f.read()

        # Check yellow banner and fallback styling
        self.assertIn('<div class="warning-banner">', content)
        self.assertIn("Graphviz not found:</strong> displaying DOT source fallback", content)
        self.assertIn("fallback-code", content)
        self.assertIn("btn-prev", content)
        self.assertIn("btn-play", content)
        self.assertIn("frame-range", content)
        self.assertIn("ArrowLeft", content)

    def test_graphviz_svg_rendering_self_contained(self):
        """When Graphviz is present, every frame renders as inline SVG without raw DOT."""
        dot_bin = find_dot()
        if not dot_bin:
            self.skipTest("Graphviz 'dot' not detected on this machine. Skipping SVG render test.")

        out_html = os.path.join(self.test_dir, "svg_out.html")
        res = subprocess.run(
            [self.python_exe, self.animate_script, "--frames", self.fixtures_dir, "--out", out_html, "--format", "svg"],
            capture_output=True,
            text=True
        )

        self.assertEqual(res.returncode, 0, f"animate.py failed: {res.stderr}")
        self.assertTrue(os.path.exists(out_html))

        with open(out_html, "r", encoding="utf-8") as f:
            content = f.read()

        # Check inline SVG
        self.assertIn("<svg", content, "HTML should contain inline <svg> element")
        self.assertIn("</svg>", content)
        # Should not show fallback banner when dot is present
        self.assertNotIn('<div class="warning-banner">', content)
        # Check controls and running cost badge
        self.assertIn("cost-badge", content)
        self.assertIn("Running MST Cost:", content)
        self.assertIn("frame-range", content)
        self.assertIn("btn-play", content)

    def test_side_by_side_comparison_mode(self):
        """Side-by-side mode renders both algorithms with shared controls and cost check."""
        left_dir = os.path.join(self.test_dir, "left_prim")
        right_dir = os.path.join(self.test_dir, "right_kruskal")
        os.makedirs(left_dir, exist_ok=True)
        os.makedirs(right_dir, exist_ok=True)

        # Generate left frames (Prim, 3 frames ending with cost 5300)
        for i in range(1, 4):
            cost = 1200 if i == 1 else (3000 if i == 2 else 5300)
            dot = f"""graph Frame {{
  label="Prim Step {i}\\nRunning Cost: {cost} | Step {i}";
  node [shape=box]; 1 -- 2 [label="100"];
}}
"""
            with open(os.path.join(left_dir, f"frame_{i:03d}.dot"), "w", encoding="utf-8") as f:
                f.write(dot)

        # Generate right frames (Kruskal, 4 frames ending with cost 5300)
        for i in range(1, 5):
            cost = 1000 if i == 1 else (2500 if i == 2 else (4000 if i == 3 else 5300))
            dot = f"""graph Frame {{
  label="Kruskal Step {i}\\nRunning Cost: {cost} | Step {i}";
  node [shape=box]; 1 -- 2 [label="100"];
}}
"""
            with open(os.path.join(right_dir, f"frame_{i:03d}.dot"), "w", encoding="utf-8") as f:
                f.write(dot)

        compare_html = os.path.join(self.test_dir, "compare_test.html")
        res = subprocess.run(
            [
                self.python_exe,
                self.animate_script,
                "--left", left_dir,
                "--right", right_dir,
                "--left-title", "Prim Algorithm",
                "--right-title", "Kruskal Algorithm",
                "--out", compare_html
            ],
            capture_output=True,
            text=True
        )

        self.assertEqual(res.returncode, 0, f"animate.py side-by-side failed: {res.stderr}")
        self.assertTrue(os.path.exists(compare_html))

        with open(compare_html, "r", encoding="utf-8") as f:
            content = f.read()

        # Both titles present
        self.assertIn("Prim Algorithm", content)
        self.assertIn("Kruskal Algorithm", content)
        # Final cost match badge
        self.assertIn("COST MATCH (IDENTICAL)", content)
        self.assertIn("5,300", content)
        # Shared controls and both viewports
        self.assertIn("left-viewport", content)
        self.assertIn("right-viewport", content)
        self.assertIn("btn-prev", content)
        self.assertIn("btn-next", content)

if __name__ == "__main__":
    unittest.main()
