#!/usr/bin/env python3
"""
Algorithm Replay Visualizer for Network Cable Optimization System (netopt)
Generates 100% self-contained, responsive HTML sliders embedding inline SVGs
(or base64 PNGs) with keyboard navigation, running cost tracking, and
side-by-side algorithm comparison mode.
"""

import os
import sys
import glob
import json
import base64
import shutil
import re
import argparse
import subprocess

def find_dot():
    """Detect Graphviz dot binary in PATH and standard installation paths."""
    if os.environ.get("NETOPT_SIMULATE_NO_DOT") == "1":
        return None

    if "GRAPHVIZ_DOT" in os.environ and os.path.isfile(os.environ["GRAPHVIZ_DOT"]):
        return os.environ["GRAPHVIZ_DOT"]

    which_dot = shutil.which("dot")
    if which_dot:
        return which_dot

    user_home = os.path.expanduser("~")
    candidates = [
        r"C:\Program Files\Graphviz\bin\dot.exe",
        r"C:\Program Files (x86)\Graphviz\bin\dot.exe",
        os.path.join(user_home, "tools", "Graphviz", "bin", "dot.exe"),
        r"C:\tools\Graphviz\bin\dot.exe",
        "/usr/bin/dot",
        "/usr/local/bin/dot",
        "/opt/homebrew/bin/dot"
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None

def print_install_hint():
    print("\n[HINT] Graphviz 'dot' executable was not found.")
    print("To enable full graphical SVG rendering:")
    print("  Windows: winget install Graphviz.Graphviz")
    print("  macOS:   brew install graphviz")
    print("  Linux:   sudo apt install graphviz\n")

def parse_dot_metadata(dot_content):
    """Extract label and running cost from dot file content."""
    label = "Graph Frame"
    running_cost = None

    # Search for label="..."
    m = re.search(r'label="([^"]+)"', dot_content)
    if m:
        raw_label = m.group(1).replace(r'\"', '"')
        lines = raw_label.split(r'\n')
        label = lines[0].strip()

        # Look for running cost in subsequent lines
        for line in lines[1:]:
            cost_m = re.search(r'Running Cost:\s*(\d+)', line)
            if cost_m:
                running_cost = int(cost_m.group(1))

    return label, running_cost

def render_frame_svg(dot_path, dot_bin):
    """Render a .dot file to an inline SVG string using dot -Tsvg."""
    try:
        proc = subprocess.run(
            [dot_bin, "-Tsvg", dot_path],
            capture_output=True,
            text=True,
            timeout=15,
            check=True
        )
        svg = proc.stdout
        # Extract <svg ... </svg>
        svg_start = svg.find("<svg")
        svg_end = svg.rfind("</svg>")
        if svg_start != -1 and svg_end != -1:
            svg_content = svg[svg_start:svg_end + 6]
            # Make SVG responsive: remove fixed width/height attributes from root <svg>
            svg_content = re.sub(r'<svg\s+([^>]*)(width="[^"]*"|height="[^"]*")', r'<svg \1', svg_content)
            svg_content = re.sub(r'<svg\s+([^>]*)(width="[^"]*"|height="[^"]*")', r'<svg \1', svg_content)
            return svg_content
        return None
    except Exception as e:
        print(f"[ERROR] Failed to render {dot_path} to SVG: {e}", file=sys.stderr)
        return None

def render_frame_png_base64(dot_path, dot_bin):
    """Render a .dot file to base64 PNG data URI."""
    try:
        proc = subprocess.run(
            [dot_bin, "-Tpng", dot_path],
            capture_output=True,
            timeout=15,
            check=True
        )
        b64 = base64.b64encode(proc.stdout).decode("ascii")
        return f"data:image/png;base64,{b64}"
    except Exception as e:
        print(f"[ERROR] Failed to render {dot_path} to PNG: {e}", file=sys.stderr)
        return None

def load_frames_from_dir(frames_dir, dot_bin, fmt="svg"):
    """Load all frame_*.dot files, sidecar metadata, and render graphics."""
    if not os.path.isdir(frames_dir):
        print(f"[ERROR] Directory does not exist: {frames_dir}", file=sys.stderr)
        return None

    dot_files = sorted(glob.glob(os.path.join(frames_dir, "frame_*.dot")))
    if not dot_files:
        # Also try any .dot files in the directory
        dot_files = sorted(glob.glob(os.path.join(frames_dir, "*.dot")))

    if not dot_files:
        print(f"[ERROR] No .dot frames found in directory: {frames_dir}", file=sys.stderr)
        return None

    # Load sidecar frames.json if available
    sidecar_map = {}
    json_path = os.path.join(frames_dir, "frames.json")
    if os.path.isfile(json_path):
        try:
            with open(json_path, "r", encoding="utf-8") as jf:
                data = json.load(jf)
                for item in data.get("frames", []):
                    fname = item.get("filename")
                    if fname:
                        sidecar_map[fname] = item
        except Exception:
            pass

    frames = []
    print(f"Processing {len(dot_files)} frames from '{frames_dir}'...")

    for i, dot_file in enumerate(dot_files):
        fname = os.path.basename(dot_file)
        with open(dot_file, "r", encoding="utf-8", errors="replace") as f:
            dot_content = f.read()

        label, running_cost = parse_dot_metadata(dot_content)

        # Check sidecar
        if fname in sidecar_map:
            sc = sidecar_map[fname]
            if sc.get("label"):
                label = sc["label"]
            if sc.get("runningCost") is not None:
                running_cost = sc["runningCost"]

        rendered_graphic = None
        if dot_bin:
            if fmt == "png":
                rendered_graphic = render_frame_png_base64(dot_file, dot_bin)
            else:
                rendered_graphic = render_frame_svg(dot_file, dot_bin)

        frames.append({
            "index": i + 1,
            "filename": fname,
            "label": label,
            "runningCost": running_cost,
            "dotSource": dot_content,
            "graphic": rendered_graphic
        })

    return frames

def build_single_replay_html(frames, dot_found, title="Algorithm Replay"):
    """Generate self-contained HTML document for a single algorithm execution."""
    frames_json_str = json.dumps(frames)
    fallback_banner = ""
    if not dot_found:
        fallback_banner = """
        <div class="warning-banner">
            ⚠️ <strong>Graphviz not found:</strong> displaying DOT source fallback. Install Graphviz (<code>winget install Graphviz.Graphviz</code> / <code>brew install graphviz</code> / <code>sudo apt install graphviz</code>) and re-run to view rendered vector graphs.
        </div>
        """

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title} - Network Cable Optimization</title>
    <style>
        :root {{
            --primary: #1976D2;
            --primary-dark: #0D47A1;
            --surface: #FFFFFF;
            --bg: #F0F4F8;
            --text: #263238;
            --border: #CFD8DC;
            --accepted: #2E7D32;
            --rejected: #D32F2F;
            --candidate: #F57C00;
        }}
        * {{ box-sizing: border-box; margin: 0; padding: 0; }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: var(--bg);
            color: var(--text);
            padding: 20px;
        }}
        .container {{
            max-width: 1000px;
            margin: 0 auto;
            background: var(--surface);
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.08);
            padding: 30px;
        }}
        header {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--border);
            padding-bottom: 15px;
            margin-bottom: 20px;
            flex-wrap: wrap;
            gap: 10px;
        }}
        h1 {{ color: var(--primary-dark); font-size: 24px; }}
        .cost-badge {{
            background: #E8F5E9;
            color: #1B5E20;
            padding: 6px 14px;
            border-radius: 20px;
            font-weight: 700;
            font-size: 15px;
            border: 1px solid #A5D6A7;
        }}
        .warning-banner {{
            background: #FFF9C4;
            color: #F57F17;
            padding: 12px 16px;
            border-radius: 8px;
            border-left: 5px solid #FBC02D;
            margin-bottom: 20px;
            font-size: 14px;
            line-height: 1.5;
        }}
        .warning-banner code {{
            background: rgba(0,0,0,0.06);
            padding: 2px 6px;
            border-radius: 4px;
            font-family: monospace;
        }}
        .step-banner {{
            background: #E3F2FD;
            color: #0D47A1;
            padding: 14px 20px;
            border-radius: 8px;
            border-left: 5px solid var(--primary);
            font-weight: 600;
            font-size: 16px;
            margin-bottom: 20px;
            min-height: 50px;
            display: flex;
            align-items: center;
        }}
        .controls {{
            display: flex;
            align-items: center;
            gap: 12px;
            background: #FAFAFA;
            padding: 15px 20px;
            border-radius: 8px;
            border: 1px solid var(--border);
            margin-bottom: 20px;
            flex-wrap: wrap;
        }}
        button {{
            padding: 9px 18px;
            background: var(--primary);
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-weight: 600;
            font-size: 14px;
            transition: background 0.15s ease;
        }}
        button:hover {{ background: var(--primary-dark); }}
        input[type=range] {{
            flex-grow: 1;
            height: 8px;
            border-radius: 4px;
            cursor: pointer;
            accent-color: var(--primary);
        }}
        .counter {{
            font-weight: 700;
            color: #546E7A;
            font-size: 14px;
            min-width: 90px;
            text-align: right;
        }}
        .legend {{
            display: flex;
            gap: 18px;
            justify-content: center;
            font-size: 13px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }}
        .legend-item {{ display: flex; align-items: center; gap: 6px; }}
        .indicator {{ width: 14px; height: 14px; border-radius: 3px; display: inline-block; }}
        .ind-accepted {{ background: var(--accepted); }}
        .ind-rejected {{ background: var(--rejected); }}
        .ind-candidate {{ background: var(--candidate); }}
        .ind-unseen {{ background: #B0BEC5; border: 1px dashed #78909C; }}
        .frame-viewport {{
            border: 1px solid var(--border);
            border-radius: 8px;
            background: #FAFAFA;
            min-height: 450px;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: auto;
        }}
        svg {{
            max-width: 100%;
            height: auto;
            max-height: 520px;
            display: block;
            margin: 0 auto;
        }}
        img {{
            max-width: 100%;
            height: auto;
            max-height: 520px;
            display: block;
            margin: 0 auto;
        }}
        pre.fallback-code {{
            text-align: left;
            background: #263238;
            color: #ECEFF1;
            padding: 20px;
            border-radius: 8px;
            font-family: Consolas, "Courier New", monospace;
            font-size: 13px;
            line-height: 1.4;
            overflow-x: auto;
            width: 100%;
            max-height: 480px;
        }}
        footer {{
            margin-top: 25px;
            text-align: center;
            font-size: 12px;
            color: #78909C;
        }}
    </style>
</head>
<body>
<div class="container">
    <header>
        <h1>{title}</h1>
        <div class="cost-badge" id="cost-badge">Running Cost: -</div>
    </header>

    {fallback_banner}

    <div class="step-banner" id="step-banner">Loading frame...</div>

    <div class="controls">
        <button id="btn-prev" onclick="prevStep()">&#9664; Prev</button>
        <button id="btn-play" onclick="togglePlay()">Play</button>
        <button id="btn-next" onclick="nextStep()">Next &#9654;</button>
        <input type="range" id="frame-range" min="0" max="{len(frames) - 1}" value="0" oninput="seekTo(this.value)">
        <span class="counter" id="frame-counter">1 / {len(frames)}</span>
    </div>

    <div class="legend">
        <div class="legend-item"><span class="indicator ind-accepted"></span> <strong>Accepted (In Tree)</strong></div>
        <div class="legend-item"><span class="indicator ind-rejected"></span> <strong>Rejected (Cycle)</strong></div>
        <div class="legend-item"><span class="indicator ind-candidate"></span> <strong>Candidate</strong></div>
        <div class="legend-item"><span class="indicator ind-unseen"></span> <strong>Unvisited</strong></div>
    </div>

    <div class="frame-viewport" id="viewport">
        <!-- Rendered Graphic or Code Fallback -->
    </div>

    <footer>
        Keyboard Shortcuts: <strong>&larr; / &rarr;</strong>: Prev / Next &bull; <strong>Space</strong>: Play / Pause &bull; <strong>Home / End</strong>: First / Last Frame
    </footer>
</div>

<script>
    const frames = {frames_json_str};
    let currentIdx = 0;
    let timer = null;

    function updateView() {{
        const f = frames[currentIdx];
        document.getElementById('step-banner').innerText = f.label;
        document.getElementById('frame-range').value = currentIdx;
        document.getElementById('frame-counter').innerText = (currentIdx + 1) + " / " + frames.length;

        const costBadge = document.getElementById('cost-badge');
        if (f.runningCost !== null && f.runningCost !== undefined) {{
            costBadge.innerText = "Running MST Cost: " + Number(f.runningCost).toLocaleString();
            costBadge.style.display = "inline-block";
        }} else {{
            costBadge.style.display = "none";
        }}

        const vp = document.getElementById('viewport');
        if (f.graphic) {{
            if (f.graphic.startsWith("data:image")) {{
                vp.innerHTML = '<img src="' + f.graphic + '" alt="Frame ' + (currentIdx + 1) + '">';
            }} else {{
                vp.innerHTML = f.graphic;
            }}
        }} else {{
            vp.innerHTML = '<pre class="fallback-code">' +
                f.dotSource.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") +
                '</pre>';
        }}
    }}

    function seekTo(val) {{
        currentIdx = parseInt(val, 10);
        updateView();
    }}

    function prevStep() {{
        if (currentIdx > 0) {{
            currentIdx--;
            updateView();
        }}
    }}

    function nextStep() {{
        if (currentIdx < frames.length - 1) {{
            currentIdx++;
            updateView();
        }} else if (timer) {{
            togglePlay();
        }}
    }}

    function togglePlay() {{
        const btn = document.getElementById('btn-play');
        if (timer) {{
            clearInterval(timer);
            timer = null;
            btn.innerText = "Play";
        }} else {{
            btn.innerText = "Pause";
            timer = setInterval(nextStep, 1000);
        }}
    }}

    // Keyboard support
    window.addEventListener('keydown', function(e) {{
        if (e.target.tagName === 'INPUT' && e.target.type !== 'range') return;
        if (e.key === "ArrowLeft") {{
            e.preventDefault();
            prevStep();
        }} else if (e.key === "ArrowRight") {{
            e.preventDefault();
            nextStep();
        }} else if (e.key === " ") {{
            e.preventDefault();
            togglePlay();
        }} else if (e.key === "Home") {{
            e.preventDefault();
            seekTo(0);
        }} else if (e.key === "End") {{
            e.preventDefault();
            seekTo(frames.length - 1);
        }}
    }});

    updateView();
</script>
</body>
</html>
"""
    return html

def build_side_by_side_html(left_frames, right_frames, dot_found, left_title="Prim's Algorithm", right_title="Kruskal's Algorithm"):
    """Generate self-contained HTML document comparing two algorithms side-by-side."""
    left_json = json.dumps(left_frames)
    right_json = json.dumps(right_frames)
    max_steps = max(len(left_frames), len(right_frames))

    left_final_cost = left_frames[-1].get("runningCost", 0) if left_frames else 0
    right_final_cost = right_frames[-1].get("runningCost", 0) if right_frames else 0
    costs_match = (left_final_cost == right_final_cost)

    match_badge_class = "match-success" if costs_match else "match-fail"
    match_badge_text = "COST MATCH (IDENTICAL)" if costs_match else "COST MISMATCH"

    fallback_banner = ""
    if not dot_found:
        fallback_banner = """
        <div class="warning-banner">
            ⚠️ <strong>Graphviz not found:</strong> displaying DOT source fallback. Install Graphviz (<code>winget install Graphviz.Graphviz</code> / <code>brew install graphviz</code> / <code>sudo apt install graphviz</code>) and re-run to view rendered graphs.
        </div>
        """

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Side-by-Side Algorithm Comparison - netopt</title>
    <style>
        :root {{
            --primary: #1976D2;
            --primary-dark: #0D47A1;
            --surface: #FFFFFF;
            --bg: #F0F4F8;
            --text: #263238;
            --border: #CFD8DC;
            --accepted: #2E7D32;
            --rejected: #D32F2F;
            --candidate: #F57C00;
        }}
        * {{ box-sizing: border-box; margin: 0; padding: 0; }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: var(--bg);
            color: var(--text);
            padding: 20px;
        }}
        .container {{
            max-width: 1400px;
            margin: 0 auto;
            background: var(--surface);
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.08);
            padding: 30px;
        }}
        header {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--border);
            padding-bottom: 15px;
            margin-bottom: 20px;
            flex-wrap: wrap;
            gap: 15px;
        }}
        h1 {{ color: var(--primary-dark); font-size: 24px; }}
        .summary-bar {{
            display: flex;
            gap: 12px;
            align-items: center;
            flex-wrap: wrap;
        }}
        .badge {{
            padding: 6px 14px;
            border-radius: 20px;
            font-weight: 700;
            font-size: 14px;
        }}
        .cost-left {{ background: #E1F5FE; color: #0277BD; border: 1px solid #81D4FA; }}
        .cost-right {{ background: #EDE7F6; color: #4527A0; border: 1px solid #B39DDB; }}
        .match-success {{ background: #E8F5E9; color: #1B5E20; border: 1px solid #A5D6A7; }}
        .match-fail {{ background: #FFEBEE; color: #C62828; border: 1px solid #EF9A9A; }}
        .warning-banner {{
            background: #FFF9C4;
            color: #F57F17;
            padding: 12px 16px;
            border-radius: 8px;
            border-left: 5px solid #FBC02D;
            margin-bottom: 20px;
            font-size: 14px;
        }}
        .controls {{
            display: flex;
            align-items: center;
            gap: 12px;
            background: #FAFAFA;
            padding: 15px 20px;
            border-radius: 8px;
            border: 1px solid var(--border);
            margin-bottom: 20px;
            flex-wrap: wrap;
        }}
        button {{
            padding: 9px 18px;
            background: var(--primary);
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-weight: 600;
            font-size: 14px;
        }}
        button:hover {{ background: var(--primary-dark); }}
        input[type=range] {{
            flex-grow: 1;
            height: 8px;
            cursor: pointer;
            accent-color: var(--primary);
        }}
        .counter {{
            font-weight: 700;
            color: #546E7A;
            font-size: 14px;
            min-width: 90px;
            text-align: right;
        }}
        .columns {{
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
        }}
        @media (max-width: 900px) {{
            .columns {{ grid-template-columns: 1fr; }}
        }}
        .col-card {{
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 20px;
            background: #FAFAFA;
            display: flex;
            flex-direction: column;
        }}
        .col-title {{
            font-size: 18px;
            font-weight: 700;
            margin-bottom: 12px;
            color: var(--primary);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }}
        .col-step {{
            background: #ECEFF1;
            padding: 8px 12px;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 600;
            margin-bottom: 15px;
            min-height: 40px;
            display: flex;
            align-items: center;
        }}
        .viewport {{
            background: white;
            border: 1px solid var(--border);
            border-radius: 6px;
            min-height: 420px;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 15px;
            overflow: auto;
            flex-grow: 1;
        }}
        svg {{
            max-width: 100%;
            height: auto;
            max-height: 480px;
            display: block;
            margin: 0 auto;
        }}
        img {{
            max-width: 100%;
            height: auto;
            max-height: 480px;
            display: block;
            margin: 0 auto;
        }}
        pre.fallback-code {{
            background: #263238;
            color: #ECEFF1;
            padding: 15px;
            border-radius: 6px;
            font-family: monospace;
            font-size: 11px;
            width: 100%;
            max-height: 400px;
            overflow: auto;
        }}
        footer {{
            margin-top: 25px;
            text-align: center;
            font-size: 12px;
            color: #78909C;
        }}
    </style>
</head>
<body>
<div class="container">
    <header>
        <h1>Side-by-Side Algorithm Replay</h1>
        <div class="summary-bar">
            <span class="badge cost-left">{left_title}: {left_final_cost:,}</span>
            <span class="badge cost-right">{right_title}: {right_final_cost:,}</span>
            <span class="badge {match_badge_class}">{match_badge_text}</span>
        </div>
    </header>

    {fallback_banner}

    <div class="controls">
        <button id="btn-prev" onclick="prevStep()">&#9664; Prev</button>
        <button id="btn-play" onclick="togglePlay()">Play</button>
        <button id="btn-next" onclick="nextStep()">Next &#9654;</button>
        <input type="range" id="frame-range" min="0" max="{max_steps - 1}" value="0" oninput="seekTo(this.value)">
        <span class="counter" id="frame-counter">1 / {max_steps}</span>
    </div>

    <div class="columns">
        <div class="col-card">
            <div class="col-title">
                <span>{left_title}</span>
                <span id="left-cost" style="font-size:14px; color:#0277BD;">Cost: -</span>
            </div>
            <div class="col-step" id="left-step">Loading...</div>
            <div class="viewport" id="left-viewport"></div>
        </div>

        <div class="col-card">
            <div class="col-title">
                <span>{right_title}</span>
                <span id="right-cost" style="font-size:14px; color:#4527A0;">Cost: -</span>
            </div>
            <div class="col-step" id="right-step">Loading...</div>
            <div class="viewport" id="right-viewport"></div>
        </div>
    </div>

    <footer>
        Keyboard Shortcuts: <strong>&larr; / &rarr;</strong>: Prev / Next &bull; <strong>Space</strong>: Play / Pause &bull; <strong>Home / End</strong>: First / Last Frame
    </footer>
</div>

<script>
    const leftFrames = {left_json};
    const rightFrames = {right_json};
    const maxSteps = {max_steps};
    let currentIdx = 0;
    let timer = null;

    function renderColumn(frames, idx, stepElemId, costElemId, vpElemId) {{
        const clampedIdx = Math.min(idx, frames.length - 1);
        const f = frames[clampedIdx];

        const stepEl = document.getElementById(stepElemId);
        const costEl = document.getElementById(costElemId);
        const vpEl = document.getElementById(vpElemId);

        if (idx >= frames.length) {{
            stepEl.innerHTML = f.label + " <em>(Execution Completed)</em>";
        }} else {{
            stepEl.innerText = f.label;
        }}

        if (f.runningCost !== null && f.runningCost !== undefined) {{
            costEl.innerText = "Running Cost: " + Number(f.runningCost).toLocaleString();
        }}

        if (f.graphic) {{
            if (f.graphic.startsWith("data:image")) {{
                vpEl.innerHTML = '<img src="' + f.graphic + '" alt="Frame">';
            }} else {{
                vpEl.innerHTML = f.graphic;
            }}
        }} else {{
            vpEl.innerHTML = '<pre class="fallback-code">' +
                f.dotSource.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") +
                '</pre>';
        }}
    }}

    function updateView() {{
        document.getElementById('frame-range').value = currentIdx;
        document.getElementById('frame-counter').innerText = (currentIdx + 1) + " / " + maxSteps;

        renderColumn(leftFrames, currentIdx, 'left-step', 'left-cost', 'left-viewport');
        renderColumn(rightFrames, currentIdx, 'right-step', 'right-cost', 'right-viewport');
    }}

    function seekTo(val) {{
        currentIdx = parseInt(val, 10);
        updateView();
    }}

    function prevStep() {{
        if (currentIdx > 0) {{
            currentIdx--;
            updateView();
        }}
    }}

    function nextStep() {{
        if (currentIdx < maxSteps - 1) {{
            currentIdx++;
            updateView();
        }} else if (timer) {{
            togglePlay();
        }}
    }}

    function togglePlay() {{
        const btn = document.getElementById('btn-play');
        if (timer) {{
            clearInterval(timer);
            timer = null;
            btn.innerText = "Play";
        }} else {{
            btn.innerText = "Pause";
            timer = setInterval(nextStep, 1000);
        }}
    }}

    window.addEventListener('keydown', function(e) {{
        if (e.target.tagName === 'INPUT' && e.target.type !== 'range') return;
        if (e.key === "ArrowLeft") {{
            e.preventDefault();
            prevStep();
        }} else if (e.key === "ArrowRight") {{
            e.preventDefault();
            nextStep();
        }} else if (e.key === " ") {{
            e.preventDefault();
            togglePlay();
        }} else if (e.key === "Home") {{
            e.preventDefault();
            seekTo(0);
        }} else if (e.key === "End") {{
            e.preventDefault();
            seekTo(maxSteps - 1);
        }}
    }});

    updateView();
</script>
</body>
</html>
"""
    return html

def main():
    parser = argparse.ArgumentParser(
        description="Render self-contained HTML execution sliders from netopt DOT frames"
    )
    # Single mode arguments
    parser.add_argument("--frames", "--dir", dest="frames_dir", help="Directory containing frame_*.dot files")
    parser.add_argument("--title", default="Algorithm Replay", help="Page title for single mode")

    # Side-by-side comparison mode arguments
    parser.add_argument("--left", dest="left_dir", help="Directory of left algorithm frames (e.g. Prim)")
    parser.add_argument("--right", dest="right_dir", help="Directory of right algorithm frames (e.g. Kruskal)")
    parser.add_argument("--left-title", default="Prim's Algorithm", help="Title for left algorithm panel")
    parser.add_argument("--right-title", default="Kruskal's Algorithm", help="Title for right algorithm panel")

    # Output options
    parser.add_argument("--out", "--output", dest="out_file", help="Destination HTML file")
    parser.add_argument("--format", choices=["svg", "png"], default="svg", help="Graphic format (svg or png)")

    args = parser.parse_args()

    dot_bin = find_dot()
    if dot_bin:
        print(f"[INFO] Using Graphviz dot: {dot_bin}")
    else:
        print("[WARNING] Graphviz 'dot' executable was not found.")
        print_install_hint()

    # Determine mode: Side-by-side or Single
    if args.left_dir and args.right_dir:
        # Side-by-side mode
        out_file = args.out_file or "compare.html"
        left_frames = load_frames_from_dir(args.left_dir, dot_bin, args.format)
        right_frames = load_frames_from_dir(args.right_dir, dot_bin, args.format)

        if not left_frames or not right_frames:
            print("[ERROR] Failed to load frames for side-by-side comparison.", file=sys.stderr)
            sys.exit(1)

        html = build_side_by_side_html(left_frames, right_frames, bool(dot_bin), args.left_title, args.right_title)
        os.makedirs(os.path.dirname(os.path.abspath(out_file)), exist_ok=True)
        with open(out_file, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"[SUCCESS] Wrote side-by-side comparison HTML to: {os.path.abspath(out_file)}")
        sys.exit(0)

    elif args.frames_dir:
        # Single replay mode
        out_file = args.out_file or "replay.html"
        frames = load_frames_from_dir(args.frames_dir, dot_bin, args.format)
        if not frames:
            print("[ERROR] No valid frames loaded.", file=sys.stderr)
            sys.exit(1)

        html = build_single_replay_html(frames, bool(dot_bin), args.title)
        os.makedirs(os.path.dirname(os.path.abspath(out_file)), exist_ok=True)
        with open(out_file, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"[SUCCESS] Wrote replay HTML to: {os.path.abspath(out_file)}")
        sys.exit(0)

    else:
        parser.print_help()
        print("\n[ERROR] You must supply either --frames <dir> or both --left <dir> and --right <dir>.", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
