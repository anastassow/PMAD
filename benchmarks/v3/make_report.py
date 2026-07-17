#!/usr/bin/env python3
"""
make_report.py (v3) — render results/summary.json into REPORT.html.
Self-contained: inline SVG charts, inline CSS/JS, no external assets.
Regenerate any time with:  python3 aggregate.py && python3 make_report.py
"""
import json, math, os, html

HERE = os.path.dirname(os.path.abspath(__file__))
S = json.load(open(os.path.join(HERE, "results", "summary.json")))

ORDER = S["order"]
SIZES = S["sizes"]

# fixed entity colors (validated 5-slot categorical palette, light/dark)
VAR = {be: f"var(--s{i+1})" for i, be in enumerate(ORDER)}

W = 760
ML, MR, MT, MB = 56, 118, 14, 34


def esc(x):
    return html.escape(str(x), quote=True)


def fnum(v, nd=2):
    if v is None:
        return "—"
    if v >= 1000:
        return f"{v:,.0f}"
    return f"{v:.{nd}f}".rstrip("0").rstrip(".")


def nice_ticks(lo, hi, n=5):
    if hi <= lo:
        hi = lo + 1
    span = hi - lo
    step = 10 ** math.floor(math.log10(span / n))
    for m in (1, 2, 2.5, 5, 10):
        if span / (step * m) <= n:
            step *= m
            break
    t0 = math.floor(lo / step) * step
    ticks = []
    t = t0
    while t <= hi + step * 0.01:
        if t >= lo - step * 0.01:
            ticks.append(round(t, 10))
        t += step
    return ticks


def rr_right(x, y, w, h, r=4):
    """bar with 4px rounded data-end, square at the baseline (left)."""
    r = min(r, w / 2, h / 2)
    return (f"M{x:.1f},{y:.1f} h{w - r:.1f} a{r},{r} 0 0 1 {r},{r} "
            f"v{h - 2 * r:.1f} a{r},{r} 0 0 1 -{r},{r} h-{w - r:.1f} z")


def legend(items):
    row = ['<div class="legend">']
    for label, color, kind in items:
        sw = (f'<span class="lk" style="background:{color}"></span>'
              if kind == "line" else
              f'<span class="sw" style="background:{color}"></span>')
        row.append(f'<span class="li">{sw}{esc(label)}</span>')
    row.append("</div>")
    return "".join(row)


def table_view(headers, rows, caption):
    out = [f'<details class="tv"><summary>Table view — {esc(caption)}</summary><table><thead><tr>']
    out += [f"<th>{esc(h)}</th>" for h in headers]
    out.append("</tr></thead><tbody>")
    for r in rows:
        out.append("<tr>" + "".join(f"<td>{esc(c)}</td>" for c in r) + "</tr>")
    out.append("</tbody></table></details>")
    return "".join(out)


def place_labels(pts, min_gap=13, top=MT, bot=None):
    """greedy vertical collision avoidance for end labels; returns adjusted ys"""
    order = sorted(range(len(pts)), key=lambda i: pts[i])
    ys = [0.0] * len(pts)
    prev = None
    for i in order:
        y = pts[i]
        if prev is not None and y < prev + min_gap:
            y = prev + min_gap
        ys[i] = y
        prev = y
    if bot:
        over = ys and max(ys) - bot
        if over and over > 0:
            for i in order:
                ys[i] -= over
    return ys


def line_chart(series, title, ytitle, log_y=False, unit=""):
    """series: list of (name, [(x_idx, value), ...]); x = SIZES positions"""
    H = 300
    pw, ph = W - ML - MR, H - MT - MB
    allv = [v for _, pts in series for _, v in pts if v is not None]
    if log_y:
        lo = math.log10(min(allv) * 0.85)
        hi = math.log10(max(allv) * 1.15)
        ticks = [t for t in (1, 2.5, 5, 10, 25, 50, 100, 200, 400, 800, 1600)
                 if lo <= math.log10(t) <= hi]
        tv = [math.log10(t) for t in ticks]
    else:
        lo, hi = 0.0, max(allv) * 1.1
        ticks = nice_ticks(lo, hi)
        tv = ticks

    def X(i):
        return ML + pw * (i / (len(SIZES) - 1))

    def Y(v):
        vv = math.log10(v) if log_y else v
        return MT + ph * (1 - (vv - lo) / (hi - lo))

    g = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="{esc(title)}">']
    for t, raw in zip(tv, ticks):
        y = MT + ph * (1 - (t - lo) / (hi - lo))
        g.append(f'<line class="grid" x1="{ML}" y1="{y:.1f}" x2="{W - MR}" y2="{y:.1f}"/>')
        g.append(f'<text class="tick" x="{ML - 8}" y="{y + 4:.1f}" text-anchor="end">{fnum(raw, 1)}</text>')
    for i, s in enumerate(SIZES):
        g.append(f'<text class="tick" x="{X(i):.1f}" y="{H - MB + 18}" text-anchor="middle">{s}B</text>')
    g.append(f'<line class="axis" x1="{ML}" y1="{MT + ph}" x2="{W - MR}" y2="{MT + ph}"/>')

    ends = []
    for name, pts in series:
        c = VAR[name]
        d = " ".join(f"{'M' if j == 0 else 'L'}{X(i):.1f},{Y(v):.1f}"
                     for j, (i, v) in enumerate(pts))
        g.append(f'<path class="ln" d="{d}" style="stroke:{c}"/>')
        li, lv = pts[-1]
        ends.append((name, c, Y(lv), lv))
        for i, v in pts:
            tip = f"{name} @{SIZES[i]}B: {fnum(v)} {unit}"
            g.append(f'<circle class="mk" cx="{X(i):.1f}" cy="{Y(v):.1f}" r="4" '
                     f'style="fill:{c}" tabindex="0" data-tip="{esc(tip)}"/>')
            g.append(f'<circle class="hit" cx="{X(i):.1f}" cy="{Y(v):.1f}" r="12" '
                     f'data-tip="{esc(tip)}"/>')
    lys = place_labels([e[2] for e in ends], bot=MT + ph)
    for (name, c, y0, lv), y in zip(ends, lys):
        if abs(y - y0) > 7:
            g.append(f'<line class="leader" x1="{W - MR + 6}" y1="{y0:.1f}" x2="{W - MR + 26}" y2="{y + 0:.1f}"/>')
        g.append(f'<circle cx="{W - MR + 30}" cy="{y:.1f}" r="4" style="fill:{c}"/>')
        g.append(f'<text class="elab" x="{W - MR + 38}" y="{y + 4:.1f}">{esc(name)} {fnum(lv, 1)}</text>')
    g.append("</svg>")
    return "".join(g)


def hbar_chart(rows, title, unit="ns/op", maxv=None):
    """rows: list of (name, value, tip_extra)"""
    bh, gap = 20, 14
    H = MT + len(rows) * (bh + gap) + 30
    pw = W - ML - MR + 40
    mv = maxv or max(v for _, v, _ in rows) * 1.12
    ticks = nice_ticks(0, mv, 5)
    g = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="{esc(title)}">']
    for t in ticks:
        x = ML + pw * t / mv
        g.append(f'<line class="grid" x1="{x:.1f}" y1="{MT}" x2="{x:.1f}" y2="{H - 26}"/>')
        g.append(f'<text class="tick" x="{x:.1f}" y="{H - 10}" text-anchor="middle">{fnum(t, 0)}</text>')
    g.append(f'<line class="axis" x1="{ML}" y1="{MT}" x2="{ML}" y2="{H - 26}"/>')
    for r, (name, v, extra) in enumerate(rows):
        y = MT + r * (bh + gap)
        w = pw * v / mv
        c = VAR[name]
        tip = f"{name}: {fnum(v)} {unit}" + (f" — {extra}" if extra else "")
        g.append(f'<path class="bar" d="{rr_right(ML, y, w, bh)}" style="fill:{c}" '
                 f'tabindex="0" data-tip="{esc(tip)}"/>')
        g.append(f'<text class="rlab" x="{ML - 8}" y="{y + bh - 5}" text-anchor="end">{esc(name)}</text>')
        g.append(f'<text class="vlab" x="{ML + w + 8:.1f}" y="{y + bh - 5}">{fnum(v)}</text>')
    g.append("</svg>")
    return "".join(g)


def dot_log_chart(groups, title, unit="ppm"):
    """groups: list of (group_label, [(name, value)]) on a log10 x axis"""
    rowh = 26
    H = MT + sum(6 + len(g[1]) * 0 + rowh for g in groups) + 6 + 30
    rows = []
    for glab, items in groups:
        rows.append(("hdr", glab, None))
        for name, v in items:
            rows.append(("dot", name, v))
    H = MT + len(rows) * rowh + 34
    allv = [v for k, _, v in rows if k == "dot" and v and v > 0]
    lo = math.floor(math.log10(min(allv)))
    hi = math.ceil(math.log10(max(allv)))
    pw = W - ML - MR + 40

    def X(v):
        return ML + pw * (math.log10(v) - lo) / (hi - lo)

    g = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="{esc(title)}">']
    for e in range(lo, hi + 1):
        x = ML + pw * (e - lo) / (hi - lo)
        lab = f"{10 ** e:,}" if e < 6 else "1M"
        g.append(f'<line class="grid" x1="{x:.1f}" y1="{MT}" x2="{x:.1f}" y2="{H - 28}"/>')
        g.append(f'<text class="tick" x="{x:.1f}" y="{H - 12}" text-anchor="middle">{lab}</text>')
    y = MT + 14
    for kind, name, v in rows:
        if kind == "hdr":
            g.append(f'<text class="ghdr" x="{ML}" y="{y}">{esc(name)}</text>')
        else:
            c = VAR[name]
            tip = f"{name}: {fnum(v, 1)} {unit}"
            g.append(f'<line class="stem" x1="{ML}" y1="{y - 4}" x2="{X(v):.1f}" y2="{y - 4}"/>')
            g.append(f'<circle class="mk" cx="{X(v):.1f}" cy="{y - 4}" r="5" style="fill:{c}" '
                     f'tabindex="0" data-tip="{esc(tip)}"/>')
            g.append(f'<circle class="hit" cx="{X(v):.1f}" cy="{y - 4}" r="13" data-tip="{esc(tip)}"/>')
            g.append(f'<text class="rlab" x="{ML - 8}" y="{y}" text-anchor="end">{esc(name)}</text>')
        y += rowh
    g.append("</svg>")
    return "".join(g)


def spark_row(drift):
    cells = []
    allv = [v for be in ORDER for v in drift.get(be, {}).get("series", [])]
    hi = max(allv) if allv else 1
    for be in ORDER:
        d = drift.get(be)
        if not d:
            continue
        sw, sh = 132, 40
        n = len(d["series"])
        pts = " ".join(f"{4 + (sw - 8) * i / (n - 1):.1f},"
                       f"{4 + (sh - 8) * (1 - v / hi):.1f}"
                       for i, v in enumerate(d["series"]))
        cells.append(
            f'<div class="spark"><div class="sphead"><span class="sw" style="background:{VAR[be]}"></span>{esc(be)}</div>'
            f'<svg viewBox="0 0 {sw} {sh}" data-tip="{esc(be)}: window P99 {fnum(d["first"])} → {fnum(d["last"])} ns (max {fnum(d["max"])})">'
            f'<polyline class="spln" points="{pts}" style="stroke:{VAR[be]}"/></svg>'
            f'<div class="spfoot">P99 {fnum(d["first"])} → <strong>{fnum(d["last"])}</strong> ns · ×{fnum(d["ratio"])}</div></div>')
    return '<div class="sparks">' + "".join(cells) + "</div>"


def stacked_startup(initc):
    bh, gap = 20, 14
    rows = [(be, initc[be]) for be in ORDER if be in initc]
    H = MT + len(rows) * (bh + gap) + 30
    pw = W - ML - MR + 40
    mv = max(e["total_ms"] for _, e in rows) * 1.15
    ticks = nice_ticks(0, mv, 5)
    g = [f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Startup cost">']
    for t in ticks:
        x = ML + pw * t / mv
        g.append(f'<line class="grid" x1="{x:.1f}" y1="{MT}" x2="{x:.1f}" y2="{H - 26}"/>')
        g.append(f'<text class="tick" x="{x:.1f}" y="{H - 10}" text-anchor="middle">{fnum(t, 1)}</text>')
    for r, (be, e) in enumerate(rows):
        y = MT + r * (bh + gap)
        wi = pw * e["init_ms"] / mv
        wf = pw * e["fill_ms"] / mv
        tip1 = f"{be} init: {fnum(e['init_ms'], 3)} ms"
        tip2 = f"{be} first fill: {fnum(e['fill_ms'], 3)} ms"
        if wi > 1:
            g.append(f'<rect class="bar" x="{ML}" y="{y}" width="{max(wi - 2, 0):.1f}" height="{bh}" '
                     f'style="fill:var(--ph-init)" tabindex="0" data-tip="{esc(tip1)}"/>')
        g.append(f'<path class="bar" d="{rr_right(ML + wi, y, wf, bh)}" '
                 f'style="fill:var(--ph-fill)" tabindex="0" data-tip="{esc(tip2)}"/>')
        g.append(f'<text class="rlab" x="{ML - 8}" y="{y + bh - 5}" text-anchor="end">{esc(be)}</text>')
        g.append(f'<text class="vlab" x="{ML + wi + wf + 8:.1f}" y="{y + bh - 5}">{fnum(e["total_ms"], 2)} ms</text>')
    g.append("</svg>")
    return "".join(g)


# ------------------------------------------------------------------ data
lat, tput, churn, mixed = S["lat"], S["tput"], S["churn"], S["mixed"]
tail, forder, initc = S["tail"], S["forder"], S["init"]
drift, mem_rss, mem_px = S["drift"], S["mem_rss"], S["mem_pmad_exact"]

env_lines = dict(
    l.split(": ", 1) for l in S.get("env", "").splitlines() if ": " in l)

pm = lat["PMAD"]["64"]
best_rival_p50 = min((lat[be]["64"]["p50"], be) for be in ORDER[1:])
mx_sorted = sorted(((mixed[be].get("mean"), be) for be in ORDER if mixed.get(be)), key=lambda t: t[0])
mixed_rank = next(i for i, (_, be) in enumerate(mx_sorted) if be == "PMAD") + 1
churn64 = churn["PMAD"]["64"]

kpis = [
    ("Hot-path P50 @64B", f"{fnum(pm['p50'])} ns",
     f"best rival {best_rival_p50[1]} {fnum(best_rival_p50[0])} ns"),
    ("Mixed-workload mean", f"{fnum(mixed['PMAD'].get('mean'))} ns",
     f"rank {mixed_rank} of {len(mx_sorted)} allocators"),
    ("Churn @64B ≥1µs stalls", f"{fnum(churn64.get('ge1us_ppm'))} ppm",
     f"system: {fnum(churn['system']['64'].get('ge1us_ppm'))} ppm"),
    ("Worst header overhead", "100%", "16B objects (16B header each)"),
]

# charts -------------------------------------------------------------
c_lat = line_chart([(be, [(i, lat[be][str(s)]["p50"]) for i, s in enumerate(SIZES)
                          if str(s) in lat.get(be, {})]) for be in ORDER],
                   "Hot-path P50 latency by size", "ns/op", log_y=True, unit="ns/op")
c_lat999 = line_chart([(be, [(i, lat[be][str(s)]["p99.9"]) for i, s in enumerate(SIZES)
                             if str(s) in lat.get(be, {})]) for be in ORDER],
                      "Hot-path P99.9 latency by size", "ns/op", log_y=True, unit="ns/op")
c_tput = line_chart([(be, [(i, tput[be][str(s)]) for i, s in enumerate(SIZES)
                           if str(s) in tput.get(be, {})]) for be in ORDER],
                    "Throughput by size", "Mops/s", log_y=True, unit="Mops/s")
c_mixed = hbar_chart([(be, mixed[be]["mean"], f"P99 {fnum(mixed[be].get('p99'))} ns")
                      for _, be in mx_sorted],
                     "Mixed multi-class churn, mean ns/op")
c_tail = dot_log_chart(
    [("Churn @64B — ops ≥100 ns (ppm, log scale)",
      [(be, churn[be]["64"].get("ge100ns_ppm")) for be in ORDER]),
     ("Churn @1024B — ops ≥100 ns (ppm, log scale)",
      [(be, churn[be]["1024"].get("ge100ns_ppm")) for be in ORDER])],
    "Tail exceedance")
c_drift = spark_row(drift)
mem_series = [("PMAD", [(i, mem_px[str(s)]) for i, s in enumerate(SIZES)])]
for be in ORDER[1:]:
    pts = [(i, max(mem_rss[be][str(s)], 0.05)) for i, s in enumerate(SIZES)
           if str(s) in mem_rss.get(be, {})]
    if pts:
        mem_series.append((be, pts))
c_mem = line_chart(mem_series, "Memory overhead by size", "%", log_y=True, unit="%")
pat_lab = {"lifo": "LIFO", "fifo": "FIFO", "rand": "random"}
c_ford = []
for pat in ("lifo", "fifo", "rand"):
    rows = sorted(((be, forder[be][pat]["pair"], f"alloc {fnum(forder[be][pat]['alloc'])} / free {fnum(forder[be][pat]['free'])}")
                   for be in ORDER if pat in forder.get(be, {})), key=lambda r: r[1])
    c_ford.append(f'<h4>{pat_lab[pat]} free order — alloc+free ns/op</h4>' +
                  hbar_chart(rows, f"Free order {pat}"))
c_init = stacked_startup(initc)

# table views --------------------------------------------------------
tv_lat = table_view(["Allocator"] + [f"{s}B" for s in SIZES],
                    [[be] + [f"{fnum(lat[be][str(s)]['p50'])} / {fnum(lat[be][str(s)]['p99.9'])}"
                             if str(s) in lat.get(be, {}) else "—" for s in SIZES]
                     for be in ORDER], "P50 / P99.9 ns per op")
tv_tput = table_view(["Allocator"] + [f"{s}B" for s in SIZES],
                     [[be] + [fnum(tput[be].get(str(s)), 1) for s in SIZES] for be in ORDER],
                     "Mops/s")
tv_mixed = table_view(["Allocator", "mean", "P50", "P99", "P99.9", "≥100ns ppm", "≥1µs ppm", "max ns"],
                      [[be, fnum(mixed[be].get("mean")), fnum(mixed[be].get("p50")),
                        fnum(mixed[be].get("p99")), fnum(mixed[be].get("p99.9")),
                        fnum(mixed[be].get("ge100ns_ppm"), 1), fnum(mixed[be].get("ge1us_ppm")),
                        fnum(mixed[be].get("max"), 0)] for be in ORDER], "ns per op")
tv_tail = table_view(["Allocator", "@64B ≥100ns", "@64B ≥1µs", "@64B max ns",
                      "@1024B ≥100ns", "@1024B ≥1µs", "@1024B max ns"],
                     [[be, fnum(churn[be]["64"].get("ge100ns_ppm"), 1), fnum(churn[be]["64"].get("ge1us_ppm")),
                       fnum(churn[be]["64"].get("max"), 0), fnum(churn[be]["1024"].get("ge100ns_ppm"), 1),
                       fnum(churn[be]["1024"].get("ge1us_ppm")), fnum(churn[be]["1024"].get("max"), 0)]
                      for be in ORDER], "exceedance ppm")
tv_mem = table_view(["Allocator"] + [f"{s}B" for s in SIZES],
                    [["PMAD (exact)"] + [f"{fnum(mem_px[str(s)], 1)}%" for s in SIZES]] +
                    [[be] + [f"{fnum(mem_rss[be].get(str(s)), 1)}%" for s in SIZES] for be in ORDER[1:]],
                    "overhead % of requested bytes")
tv_ford = table_view(["Allocator", "LIFO a/f", "FIFO a/f", "RAND a/f"],
                     [[be] + [f"{fnum(forder[be][p]['alloc'])} / {fnum(forder[be][p]['free'])}"
                              if p in forder.get(be, {}) else "—" for p in ("lifo", "fifo", "rand")]
                      for be in ORDER], "ns per op")
tv_init = table_view(["Allocator", "init ms", "first fill ms", "total ms"],
                     [[be, fnum(initc[be]["init_ms"], 3), fnum(initc[be]["fill_ms"], 3),
                       fnum(initc[be]["total_ms"], 3)] for be in ORDER if be in initc], "milliseconds")
tv_drift = table_view(["Allocator", "first window P99", "last", "max", "last/first"],
                      [[be, fnum(drift[be]["first"]), fnum(drift[be]["last"]),
                        fnum(drift[be]["max"]), f"{fnum(drift[be]['ratio'])}×"]
                       for be in ORDER if be in drift], "ns per op")

leg_all = legend([(be, VAR[be], "line") for be in ORDER])
leg_init = legend([("pool init (mmap + carve)", "var(--ph-init)", "sw"),
                   ("first fill (1M allocs + page faults)", "var(--ph-fill)", "sw")])

kpi_html = "".join(
    f'<div class="tile"><div class="tlabel">{esc(l)}</div>'
    f'<div class="tvalue">{esc(v)}</div><div class="tdelta">{esc(d)}</div></div>'
    for l, v, d in kpis)

date = env_lines.get("date", "")
cpu = env_lines.get("cpu", "")
git = env_lines.get("git", "")

page = f"""<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PMAD v3 benchmark report</title>
<style>
.viz-root {{
  color-scheme: light;
  --surface-1:#fcfcfb; --page:#f9f9f7;
  --ink:#0b0b0b; --ink-2:#52514e; --muted:#898781;
  --grid:#e1e0d9; --axis:#c3c2b7; --ring:rgba(11,11,11,.10);
  --s1:#2a78d6; --s2:#008300; --s3:#e87ba4; --s4:#eda100; --s5:#1baf7a;
  --ph-init:#86b6ef; --ph-fill:#2a78d6;
  --good:#006300; --bad:#d03b3b;
}}
@media (prefers-color-scheme: dark) {{
  :root:where(:not([data-theme="light"])) .viz-root {{
    color-scheme: dark;
    --surface-1:#1a1a19; --page:#0d0d0d;
    --ink:#ffffff; --ink-2:#c3c2b7; --muted:#898781;
    --grid:#2c2c2a; --axis:#383835; --ring:rgba(255,255,255,.10);
    --s1:#3987e5; --s2:#008300; --s3:#d55181; --s4:#c98500; --s5:#199e70;
    --ph-init:#1c5cab; --ph-fill:#3987e5;
    --good:#0ca30c;
  }}
}}
:root[data-theme="dark"] .viz-root {{
  color-scheme: dark;
  --surface-1:#1a1a19; --page:#0d0d0d;
  --ink:#ffffff; --ink-2:#c3c2b7; --muted:#898781;
  --grid:#2c2c2a; --axis:#383835; --ring:rgba(255,255,255,.10);
  --s1:#3987e5; --s2:#008300; --s3:#d55181; --s4:#c98500; --s5:#199e70;
  --ph-init:#1c5cab; --ph-fill:#3987e5;
  --good:#0ca30c;
}}
.viz-root {{
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  background: var(--page); color: var(--ink);
  margin: 0; padding: 28px 16px 80px; line-height: 1.5;
}}
.wrap {{ max-width: 900px; margin: 0 auto; }}
h1 {{ font-size: 26px; margin: 0 0 4px; }}
h2 {{ font-size: 19px; margin: 40px 0 6px; }}
h3 {{ font-size: 16px; margin: 26px 0 6px; }}
h4 {{ font-size: 13.5px; margin: 18px 0 2px; color: var(--ink-2); font-weight:600; }}
.sub {{ color: var(--ink-2); font-size: 13.5px; margin-bottom: 6px; }}
.envl {{ color: var(--muted); font-size: 12.5px; }}
.card {{
  background: var(--surface-1); border: 1px solid var(--ring);
  border-radius: 10px; padding: 18px 20px 12px; margin: 14px 0;
}}
.note {{ font-size: 13px; color: var(--ink-2); margin: 4px 0 10px; }}
svg {{ width: 100%; height: auto; display: block; }}
.grid {{ stroke: var(--grid); stroke-width: 1; }}
.axis {{ stroke: var(--axis); stroke-width: 1; }}
.tick {{ fill: var(--muted); font-size: 11px; font-variant-numeric: tabular-nums; }}
.ln {{ fill: none; stroke-width: 2; stroke-linejoin: round; stroke-linecap: round; }}
.spln {{ fill: none; stroke-width: 2; stroke-linejoin: round; stroke-linecap: round; }}
.mk {{ stroke: var(--surface-1); stroke-width: 2; }}
.mk:focus {{ outline: 2px solid var(--ink-2); outline-offset: 2px; }}
.hit {{ fill: transparent; pointer-events: all; }}
.bar:focus {{ outline: 2px solid var(--ink-2); outline-offset: 2px; }}
.bar:hover {{ filter: brightness(1.12); }}
.stem {{ stroke: var(--grid); stroke-width: 1; }}
.leader {{ stroke: var(--axis); stroke-width: 1; }}
.elab, .rlab, .vlab {{ fill: var(--ink-2); font-size: 11.5px; }}
.vlab {{ font-variant-numeric: tabular-nums; }}
.ghdr {{ fill: var(--ink); font-size: 12px; font-weight: 600; }}
.legend {{ display: flex; flex-wrap: wrap; gap: 14px; margin: 4px 0 10px; font-size: 12.5px; color: var(--ink-2); }}
.li {{ display: inline-flex; align-items: center; gap: 6px; }}
.sw {{ width: 11px; height: 11px; border-radius: 3px; display: inline-block; }}
.lk {{ width: 16px; height: 3px; border-radius: 2px; display: inline-block; }}
.tiles {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(190px, 1fr)); gap: 12px; margin: 18px 0; }}
.tile {{ background: var(--surface-1); border: 1px solid var(--ring); border-radius: 10px; padding: 14px 16px; }}
.tlabel {{ font-size: 12.5px; color: var(--ink-2); }}
.tvalue {{ font-size: 27px; font-weight: 600; margin: 2px 0; }}
.tdelta {{ font-size: 12px; color: var(--muted); }}
.tv {{ margin: 8px 0 4px; font-size: 13px; }}
.tv summary {{ cursor: pointer; color: var(--ink-2); }}
.tv table {{ border-collapse: collapse; margin-top: 8px; width: 100%; overflow-x: auto; display: block; }}
.tv th, .tv td {{ text-align: right; padding: 4px 10px; border-bottom: 1px solid var(--grid); font-variant-numeric: tabular-nums; white-space: nowrap; }}
.tv th:first-child, .tv td:first-child {{ text-align: left; }}
.sparks {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 10px 0; }}
.spark {{ background: var(--surface-1); border: 1px solid var(--ring); border-radius: 8px; padding: 10px 12px; }}
.sphead {{ font-size: 12.5px; color: var(--ink-2); display: flex; align-items: center; gap: 6px; margin-bottom: 4px; }}
.spfoot {{ font-size: 11.5px; color: var(--muted); margin-top: 4px; }}
.verdict li {{ margin: 6px 0; }}
.win {{ color: var(--good); font-weight: 600; }}
.lose {{ color: var(--bad); font-weight: 600; }}
.caveat {{ border-left: 3px solid var(--s4); padding-left: 12px; }}
#tooltip {{
  position: fixed; pointer-events: none; z-index: 10; display: none;
  background: var(--surface-1); color: var(--ink); border: 1px solid var(--ring);
  border-radius: 6px; padding: 6px 10px; font-size: 12.5px;
  box-shadow: 0 2px 10px rgba(0,0,0,.18); max-width: 300px;
}}
</style>
<div class="viz-root"><div class="wrap">
<h1>PMAD — v3 benchmark report</h1>
<div class="sub">Pool-based fixed-size-class allocator vs system malloc, jemalloc, tcmalloc, mimalloc — single-threaded, {esc(cpu)}</div>
<div class="envl">{esc(date)} · commit {esc(git)} · medians across repetitions · raw data in <code>benchmarks/v3/raw/</code>, regenerate with <code>aggregate.py + make_report.py</code></div>

<div class="tiles">{kpi_html}</div>

<h2>Verdict — is PMAD actually good?</h2>
<div class="card verdict">
<p><strong>Genuinely excellent inside a narrow envelope; measurably beatable outside it.</strong>
PMAD is a fixed-capacity size-class pool allocator: every alloc is a free-list pop, every free
is a push. On the workloads that match that shape — hot fixed-size allocation ≤&nbsp;128&nbsp;B, bounded
object count, one thread — it is the fastest or tied-fastest allocator in this field, with the
most bounded worst case. On broader workloads, modern general-purpose allocators catch up and
in some cases pass it.</p>
<ul>
<li><span class="win">Wins — hot path:</span> P50 {fnum(pm["p50"])} ns/op, dead flat from 16&nbsp;B to 1024&nbsp;B (statistical tie with mimalloc's {fnum(best_rival_p50[0])} ns at 64&nbsp;B; tcmalloc 3.91, jemalloc 6.53, system {fnum(lat["system"]["64"]["p50"])}). No other allocator is this size-invariant.</li>
<li><span class="win">Wins — small-block throughput:</span> 538–560 Mops/s at ≤64&nbsp;B — ~4× system/jemalloc, ~2× tcmalloc, ahead of mimalloc.</li>
<li><span class="win">Wins — bounded worst case &amp; stability:</span> the smallest max single-op latency in the mixed workload ({fnum(mixed["PMAD"].get("max"), 0)} ns vs 25,750–504,958 ns for the rivals), flat per-window P99 over a 64M-op run (0.89×; mimalloc degraded 2.16×), and capacity/layout/overhead that are exact and computable up front. No coalescing, no page-cache growth, nothing to hiccup.</li>
<li><span class="lose">Beaten — realistic mixed workload:</span> with all 7 size classes live (the new v3 test), PMAD's mean is {fnum(mixed["PMAD"].get("mean"))} ns/op — rank {mixed_rank} of {len(mx_sorted)}, behind tcmalloc ({fnum(mx_sorted[0][0])}), mimalloc and jemalloc, ahead only of system malloc. Once the working set spans many classes, PMAD's pointer-chasing free lists lose their cache advantage.</li>
<li><span class="lose">Loses — scrambled free order:</span> after 1M blocks are freed in <em>random</em> order, PMAD's next allocation pass costs ~80&nbsp;ns/op — 8–30× worse than every rival — because allocation order permanently inherits free order (nothing re-localises the list). LIFO/FIFO patterns stay at ~2&nbsp;ns. This is the architecture's sharpest edge for long-running, irregular workloads.</li>
<li><span class="lose">Loses — memory at small sizes:</span> a 16-byte header on every block means <strong>100% overhead at 16&nbsp;B</strong> (25% at 64&nbsp;B, 1.6% at 1024&nbsp;B) while tcmalloc/mimalloc measure &lt;1% RSS overhead — and the whole pool is committed up front whether used or not.</li>
<li><span class="lose">Loses — flexibility &amp; safety:</span> nothing above 1024&nbsp;B, fixed capacity (NULL on exhaustion, no growth, no fallback), no realloc/calloc, single global instance, <strong>not thread-safe</strong>, no double-free detection, and a misaligned in-pool pointer to free corrupts the list.</li>
</ul>
<p><strong>Bottom line:</strong> use PMAD as what it is — an embedded-style deterministic object pool.
For a game loop, packet buffers, or a fixed-rate event system with known sizes and counts, it beats
every general-purpose allocator here on speed <em>and</em> predictability. As a drop-in general heap,
tcmalloc or mimalloc are simply better today.</p>
<p class="note">Fair-comparison note: all timed benchmarks stay within PMAD's supported 16–1024&nbsp;B range.
v2's 4096&nbsp;B PMAD rows are invalid for current code — after the size cap changed to 1024&nbsp;B they
silently measured an instantly-failing NULL path (hence the impossible "851&nbsp;Mops/s"). The v3 harness
refuses out-of-range sizes instead.</p>
</div>

<h2>1 · Hot-path latency by size</h2>
<div class="card">
<div class="note">Alloc+free round-trip, blocks of 32 ops, 16M ops/run, median of 3 runs. Log y-axis.</div>
{leg_all}
<h4>P50 ns/op</h4>{c_lat}
<h4>P99.9 ns/op</h4>{c_lat999}
{tv_lat}
</div>

<h2>2 · Throughput</h2>
<div class="card">
<div class="note">Uninstrumented batch alloc/free (100k batch × 200 rounds), Mops/s, log y-axis. PMAD leads at ≤64&nbsp;B, then drops sharply at 128&nbsp;B+ (walking larger blocks costs a cache miss per pop) — mimalloc overtakes it from 128&nbsp;B to 512&nbsp;B. PMAD stays ahead of system malloc and jemalloc at every size.</div>
{leg_all}
{c_tput}
{tv_tput}
</div>

<h2>3 · Mixed multi-class workload (new in v3)</h2>
<div class="card">
<div class="note">262,144 live objects, weighted size stream (30% 16B … 5% 1024B), random replace, 32M ops.
PMAD runs all 7 size classes in one pool — the configuration real use would need. Lower is better.
tcmalloc wins on the mean; PMAD is mid-pack on average but has the tightest worst case (max column in the table).</div>
{c_mixed}
{tv_mixed}
</div>

<h2>4 · Tail behaviour under churn</h2>
<div class="card">
<div class="note">Single-op timing (K=1): fraction of individual operations at or above 100 ns, in parts per million, log scale. Dots further left are better.</div>
{c_tail}
{tv_tail}
</div>

<h2>5 · Latency drift over a long run</h2>
<div class="card">
<div class="note">One 64M-op churn run per allocator; per-window (640k ops) P99. A flat line means performance does not degrade as the heap ages.</div>
{c_drift}
{tv_drift}
</div>

<h2>6 · Free-order sensitivity</h2>
<div class="card">
<div class="note">1M blocks of 64&nbsp;B allocated, freed in LIFO / FIFO / random order, re-allocated; 10 rounds. Lower is better.
This is PMAD's sharpest result in both directions: fastest of all five under LIFO/FIFO (~2&nbsp;ns), and by far the slowest under random free order (~80&nbsp;ns alloc) — a pure free-list allocator inherits the free order as its future allocation order, and nothing ever re-localises it. mimalloc's sharded design is nearly order-immune.</div>
{"".join(c_ford)}
{tv_ford}
</div>

<h2>7 · Startup cost</h2>
<div class="card">
<div class="note">Time from nothing to 1M live 64&nbsp;B objects. PMAD pays once, up front, deterministically (mmap + carving every block); malloc-family allocators pay during the first fill via page faults and metadata growth.</div>
{leg_init}
{c_init}
{tv_init}
</div>

<h2>8 · Memory overhead</h2>
<div class="card">
<div class="note">PMAD: exact metadata overhead (16&nbsp;B header per block — deterministic, so computed, not sampled). Others: measured RSS delta vs requested bytes for 500k live objects. Log y-axis, % of requested memory. PMAD additionally commits its whole pool up front — this chart shows per-object overhead only.</div>
{leg_all}
{c_mem}
{tv_mem}
</div>

<h2>Method &amp; caveats</h2>
<div class="card caveat">
<ul>
<li>Single-threaded only: PMAD has no locking, so a multi-threaded comparison would be meaningless (it would crash or corrupt). The rivals' multi-thread scalability is a real advantage this suite cannot show.</li>
<li>One harness source compiled per backend; timed regions byte-identical; no LTO, so every allocator pays a real call. Timer floor calibrated and subtracted; Apple timer tick is 41.67&nbsp;ns, so sub-tick distributions use 32-op blocks and single-op runs report only ≥100&nbsp;ns exceedance (exact).</li>
<li>PMAD pools are sized with 2× headroom: exhausting a class inside a timed loop would measure the error path (which currently also does an <code>fprintf</code> to stderr in <code>pmad_alloc</code> — worth guarding for production builds).</li>
<li>Free of a pointer requires walking the pool list (<code>pointer_in_pool</code>) — O(1) here with one pool, but linear in pool count if pools were ever chained.</li>
<li>Numbers are medians across repetitions on {esc(cpu)}; absolute values move with machine load, ratios are the stable signal.</li>
</ul>
</div>

<div class="envl" style="margin-top:28px">Full tables: <code>results/tables.md</code> · raw: <code>raw/all_results.txt</code> · correctness suite: <code>benchmarks/v2/pmad_test.c</code> (19/19 pass on this commit)</div>
</div></div>
<div id="tooltip" role="status"></div>
<script>
(function () {{
  var tip = document.getElementById('tooltip');
  function show(e, text) {{
    tip.textContent = text;
    tip.style.display = 'block';
    var x = Math.min(e.clientX + 14, window.innerWidth - tip.offsetWidth - 8);
    var y = Math.min(e.clientY + 14, window.innerHeight - tip.offsetHeight - 8);
    tip.style.left = x + 'px';
    tip.style.top = y + 'px';
  }}
  document.addEventListener('pointermove', function (e) {{
    var t = e.target.closest('[data-tip]');
    if (t) show(e, t.getAttribute('data-tip'));
    else tip.style.display = 'none';
  }});
  document.addEventListener('focusin', function (e) {{
    var t = e.target.closest('[data-tip]');
    if (!t) {{ tip.style.display = 'none'; return; }}
    var r = t.getBoundingClientRect();
    tip.textContent = t.getAttribute('data-tip');
    tip.style.display = 'block';
    tip.style.left = Math.min(r.right + 8, window.innerWidth - 280) + 'px';
    tip.style.top = (r.top - 4) + 'px';
  }});
  document.addEventListener('focusout', function () {{ tip.style.display = 'none'; }});
}})();
</script>
"""

out = os.path.join(HERE, "REPORT.html")
with open(out, "w") as f:
    f.write(page)
print("wrote", out)
