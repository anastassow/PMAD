#!/usr/bin/env python3
"""
aggregate.py — derive every table in REPORT.md from the committed raw data.
Reads raw/all_results.txt (+ .drift) and raw/churn_windows.txt; writes
markdown tables to results/. Numbers are medians across repetitions (#9),
traced back to raw output (#12).
"""
import re, statistics, sys, os, glob
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
RAW  = os.path.join(HERE, "raw")
RES  = os.path.join(HERE, "results")
os.makedirs(RES, exist_ok=True)

ORDER = ["PMAD", "system", "jemalloc", "tcmalloc", "mimalloc"]

def parse_line(line):
    # strip leading "rep=N "
    m = re.match(r"rep=(\d+)\s+", line)
    rep = int(m.group(1)) if m else None
    line = line[m.end():] if m else line
    if not line.startswith("RESULT"):
        return None
    toks = line.split()
    tag = toks[1]
    d = {"_tag": tag, "_rep": rep}
    for t in toks[2:]:
        if "=" not in t:
            continue
        k, v = t.split("=", 1)
        # exceedance values look like 123(4.5ppm); keep the count and the ppm
        mm = re.match(r"^(-?\d+(?:\.\d+)?)(?:\(([-\d.]+)ppm\))?$", v)
        if mm:
            d[k] = float(mm.group(1))
            if mm.group(2) is not None:
                d[k + "_ppm"] = float(mm.group(2))
        else:
            d[k] = v
    return d

def load_results():
    # Only the rep-tagged main results feed the medians. The .drift file holds
    # the dedicated drift run (its trend is read separately from churn_windows).
    rows = []
    for fn in ["all_results.txt"]:
        p = os.path.join(RAW, fn)
        if not os.path.exists(p):
            continue
        for line in open(p):
            line = line.strip()
            if not line:
                continue
            d = parse_line(line if line.startswith("rep=") else "rep=0 " + line)
            if d:
                rows.append(d)
    return rows

def med(rows, key):
    vals = [r[key] for r in rows if key in r and isinstance(r[key], (int, float))]
    return statistics.median(vals) if vals else float("nan")

def _eq(a, b):
    try:
        return float(a) == float(b)
    except (TypeError, ValueError):
        return str(a) == str(b)

def group(rows, tag, **filt):
    out = defaultdict(list)
    for r in rows:
        if r["_tag"] != tag:
            continue
        if all(_eq(r.get(k), v) for k, v in filt.items()):
            out[r.get("backend")].append(r)
    return out

def be_sorted(keys):
    return [b for b in ORDER if b in keys] + [b for b in keys if b not in ORDER]

def fmt(x, nd=2):
    try:
        return f"{x:.{nd}f}"
    except Exception:
        return str(x)

rows = load_results()
out = []
def w(s=""): out.append(s)

# ---------------------------------------------------------------- Bench 1
w("## Bench 1 — Tail-latency distribution @ 64 B (headline)\n")
w("Body percentiles (block-amortised, K=32; median across reps), ns/op:\n")
w("| Allocator | mean | P50 | P90 | P99 | P99.9 |")
w("|---|--:|--:|--:|--:|--:|")
g = group(rows, "LAT", size="64", K="32")
for be in be_sorted(g):
    r = g[be]
    w(f"| {be} | {fmt(med(r,'mean'))} | {fmt(med(r,'p50'))} | {fmt(med(r,'p90'))} | {fmt(med(r,'p99'))} | {fmt(med(r,'p99.9'))} |")
w("\nTrue single-op tail (K=1; tail percentiles quantised to 41 ns, exceedance counts exact), ns/op:\n")
w("| Allocator | P99.9 | P99.99 | max | ≥100ns (ppm) | ≥1µs (ppm) | ≥10µs (ppm) |")
w("|---|--:|--:|--:|--:|--:|--:|")
g1 = group(rows, "LAT", size="64", K="1")
for be in be_sorted(g1):
    r = g1[be]
    w(f"| {be} | {fmt(med(r,'p99.9'))} | {fmt(med(r,'p99.99'))} | {fmt(med(r,'max'),0)} "
      f"| {fmt(med(r,'ge100ns_ppm'),1)} | {fmt(med(r,'ge1us_ppm'),2)} | {fmt(med(r,'ge10us_ppm'),3)} |")

# ---------------------------------------------------------------- Bench 2
w("\n## Bench 2 — Throughput (uninstrumented batch), Mops/s\n")
sizes = ["16","64","256","1024","4096"]
w("| Allocator | " + " | ".join(f"{s}B" for s in sizes) + " |")
w("|---|" + "--:|"*len(sizes))
for be in ORDER:
    cells=[]
    for s in sizes:
        g = group(rows, "TPUT", size=s)
        cells.append(fmt(med(g.get(be,[]),"Mops_per_s"),1) if be in g else "—")
    w(f"| {be} | " + " | ".join(cells) + " |")

# ---------------------------------------------------------------- Bench 3
w("\n## Bench 3 — Latency by block size (K=32 body), P50 / P99.9 ns/op\n")
w("| Allocator | " + " | ".join(f"{s}B" for s in sizes) + " |")
w("|---|" + "--:|"*len(sizes))
for be in ORDER:
    cells=[]
    for s in sizes:
        g = group(rows, "LAT", size=s, K="32")
        if be in g:
            cells.append(f"{fmt(med(g[be],'p50'))} / {fmt(med(g[be],'p99.9'))}")
        else:
            cells.append("—")
    w(f"| {be} | " + " | ".join(cells) + " |")

# ---------------------------------------------------------------- Bench 4
for s in ["64","1024"]:
    w(f"\n## Bench 4 — Churn @ {s} B, working set {os.environ.get('CHURN_W','262144')} objects\n")
    w("Steady-state distribution (K=32 body; median across reps), ns/op:\n")
    w("| Allocator | mean | P50 | P90 | P99 | P99.9 |")
    w("|---|--:|--:|--:|--:|--:|")
    g = group(rows, "CHURN", size=s, K="32")
    for be in be_sorted(g):
        r=g[be]
        w(f"| {be} | {fmt(med(r,'mean'))} | {fmt(med(r,'p50'))} | {fmt(med(r,'p90'))} | {fmt(med(r,'p99'))} | {fmt(med(r,'p99.9'))} |")
    w("\nSingle-op tail exceedance (K=1), fraction of ops over threshold:\n")
    w("| Allocator | ≥100ns (ppm) | ≥1µs (ppm) | ≥10µs (ppm) | max (ns) |")
    w("|---|--:|--:|--:|--:|")
    g1 = group(rows, "CHURN", size=s, K="1")
    for be in be_sorted(g1):
        r=g1[be]
        w(f"| {be} | {fmt(med(r,'ge100ns_ppm'),1)} | {fmt(med(r,'ge1us_ppm'),2)} | {fmt(med(r,'ge10us_ppm'),3)} | {fmt(med(r,'max'),0)} |")

# drift: first vs last window P99 (64B)
w("\n### Bench 4 — Drift over a single long run @64B (per-window P99, ns/op)\n")
win = defaultdict(list)
wp = os.path.join(RAW, "churn_windows.txt")
if os.path.exists(wp):
    for line in open(wp):
        t = line.split()
        if len(t) >= 8 and t[0] == "WIN":
            be, sz, widx = t[1], t[2], int(t[3])
            p50,p99,p999,mx = map(float, t[5:9])
            if sz == "64":
                win[be].append((widx,p50,p99,p999,mx))
w("| Allocator | first window P99 | last window P99 | windows-max P99 | drift (last/first) |")
w("|---|--:|--:|--:|--:|")
for be in ORDER:
    if be not in win: continue
    ws = sorted(win[be])
    first=ws[0][2]; last=ws[-1][2]; mx=max(x[2] for x in ws)
    drift = last/first if first>0 else float('nan')
    w(f"| {be} | {fmt(first)} | {fmt(last)} | {fmt(mx)} | {fmt(drift)}× |")

# ASCII sparklines of per-window P99 across the run (common scale => comparable)
if win:
    spark = "▁▂▃▄▅▆▇█"
    allv = [x[2] for be in win for x in win[be]]
    lo, hi = min(allv), max(allv)
    rng = (hi-lo) or 1.0
    w("\nPer-window P99 across the long run (left=op 0 → right=op 64M), "
      f"common scale {fmt(lo)}→{fmt(hi)} ns/op:\n")
    w("```")
    for be in ORDER:
        if be not in win: continue
        ws = sorted(win[be])
        # downsample to ~60 columns
        cols = 60
        n = len(ws)
        line = ""
        for c in range(cols):
            i = min(n-1, c*n//cols)
            v = ws[i][2]
            idx = int((v-lo)/rng*(len(spark)-1))
            line += spark[max(0,min(len(spark)-1, idx))]
        w(f"{be:9s} {line}  (last {fmt(ws[-1][2])})")
    w("```")

# ---------------------------------------------------------------- Bench 5
w("\n## Bench 5 — Memory overhead by block size\n")
w("PMAD metadata overhead (exact, deterministic — 16-byte header):\n")
w("| Size (B) | Header (B) | Blocks/64MB | Header overhead % |")
w("|--:|--:|--:|--:|")
csv = os.path.join(RES, "pmad_overhead.csv")
if os.path.exists(csv):
    for line in open(csv):
        if line.startswith("#") or line.startswith("size_B"): continue
        c=line.strip().split(",")
        if len(c)>=6:
            w(f"| {c[0]} | {c[1]} | {c[2]} | {c[5]} |")
w("\nMeasured RSS overhead, malloc-family (bytes resident per live object vs requested):\n")
w("| Allocator | " + " | ".join(f"{s}B" for s in sizes) + " |")
w("|---|" + "--:|"*len(sizes))
for be in ["system","jemalloc","tcmalloc","mimalloc"]:
    cells=[]
    for s in sizes:
        g = group(rows, "MEM", size=s)
        if be in g:
            cells.append(f"{fmt(med(g[be],'overhead_pct'),1)}%")
        else:
            cells.append("—")
    w(f"| {be} | " + " | ".join(cells) + " |")

text = "\n".join(out) + "\n"
open(os.path.join(RES, "tables.md"), "w").write(text)
print(text)
