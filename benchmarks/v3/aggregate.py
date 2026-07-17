#!/usr/bin/env python3
"""
aggregate.py (v3) — derive every table and the report's data blob from the
committed raw output. Reads raw/all_results.txt (+ .drift) and
raw/churn_windows.txt; writes results/tables.md and results/summary.json.
Numbers are medians across repetitions, traced back to raw output.
"""
import json, os, re, statistics
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
RAW  = os.path.join(HERE, "raw")
RES  = os.path.join(HERE, "results")
os.makedirs(RES, exist_ok=True)

ORDER = ["PMAD", "system", "jemalloc", "tcmalloc", "mimalloc"]
SIZES = [16, 32, 64, 128, 256, 512, 1024]

def parse_line(line):
    m = re.match(r"rep=(\d+)\s+", line)
    rep = int(m.group(1)) if m else None
    line = line[m.end():] if m else line
    if not line.startswith("RESULT"):
        return None
    toks = line.split()
    d = {"_tag": toks[1], "_rep": rep}
    for t in toks[2:]:
        if "=" not in t:
            continue
        k, v = t.split("=", 1)
        mm = re.match(r"^(-?\d+(?:\.\d+)?)(?:\(([-\d.]+)ppm\))?$", v)
        if mm:
            d[k] = float(mm.group(1))
            if mm.group(2) is not None:
                d[k + "_ppm"] = float(mm.group(2))
        else:
            d[k] = v
    return d

rows = []
for fname in ("all_results.txt", "all_results.txt.drift"):
    path = os.path.join(RAW, fname)
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                d = parse_line(line.strip())
                if d:
                    d["_drift"] = fname.endswith(".drift")
                    rows.append(d)

def med(group, key):
    vals = [r[key] for r in group if key in r]
    return statistics.median(vals) if vals else None

def sel(tag, **kw):
    out = []
    for r in rows:
        if r["_tag"] != tag or r.get("_drift"):
            continue
        ok = True
        for k, v in kw.items():
            if r.get(k) != v:
                ok = False
                break
        if ok:
            out.append(r)
    return out

summary = {"order": ORDER, "sizes": SIZES}

# ---- LAT by size (K=32) --------------------------------------------------
lat = {}
for be in ORDER:
    lat[be] = {}
    for s in SIZES:
        g = sel("LAT", backend=be, size=float(s), K=32.0)
        if g:
            lat[be][str(s)] = {k: med(g, k) for k in
                               ("mean", "p50", "p90", "p99", "p99.9", "p99.99")}
summary["lat"] = lat

# ---- single-op tail (K=1) ------------------------------------------------
tail = {}
for be in ORDER:
    tail[be] = {}
    for s in (64, 1024):
        g = sel("LAT", backend=be, size=float(s), K=1.0)
        if g:
            tail[be][str(s)] = {
                "p99.9": med(g, "p99.9"), "p99.99": med(g, "p99.99"),
                "max": med(g, "max"),
                "ge100ns_ppm": med(g, "ge100ns_ppm"),
                "ge1us_ppm": med(g, "ge1us_ppm"),
                "ge10us_ppm": med(g, "ge10us_ppm")}
summary["tail"] = tail

# ---- TPUT by size --------------------------------------------------------
tput = {}
for be in ORDER:
    tput[be] = {}
    for s in SIZES:
        g = sel("TPUT", backend=be, size=float(s))
        if g:
            tput[be][str(s)] = med(g, "Mops_per_s")
summary["tput"] = tput

# ---- CHURN ---------------------------------------------------------------
churn = {}
for be in ORDER:
    churn[be] = {}
    for s in (64, 1024):
        body = sel("CHURN", backend=be, size=float(s), K=32.0)
        tl   = sel("CHURN", backend=be, size=float(s), K=1.0)
        e = {}
        if body:
            e.update({k: med(body, k) for k in ("mean", "p50", "p90", "p99", "p99.9")})
        if tl:
            e.update({"ge100ns_ppm": med(tl, "ge100ns_ppm"),
                      "ge1us_ppm": med(tl, "ge1us_ppm"),
                      "ge10us_ppm": med(tl, "ge10us_ppm"),
                      "max": med(tl, "max")})
        churn[be][str(s)] = e
summary["churn"] = churn

# ---- MIXED ---------------------------------------------------------------
mixed = {}
for be in ORDER:
    body = sel("MIXED", backend=be, K=32.0)
    tl   = sel("MIXED", backend=be, K=1.0)
    e = {}
    if body:
        e.update({k: med(body, k) for k in ("mean", "p50", "p90", "p99", "p99.9")})
    if tl:
        e.update({"ge100ns_ppm": med(tl, "ge100ns_ppm"),
                  "ge1us_ppm": med(tl, "ge1us_ppm"),
                  "ge10us_ppm": med(tl, "ge10us_ppm"),
                  "max": med(tl, "max")})
    mixed[be] = e
summary["mixed"] = mixed

# ---- FORDER --------------------------------------------------------------
forder = {}
for be in ORDER:
    forder[be] = {}
    for pat in ("lifo", "fifo", "rand"):
        g = sel("FORDER", backend=be, pattern=pat)
        if g:
            forder[be][pat] = {"alloc": med(g, "alloc_ns_op"),
                               "free": med(g, "free_ns_op"),
                               "pair": med(g, "pair_ns_op")}
summary["forder"] = forder

# ---- INIT ----------------------------------------------------------------
initc = {}
for be in ORDER:
    g = sel("INIT", backend=be)
    if g:
        initc[be] = {"init_ms": med(g, "init_ms"), "fill_ms": med(g, "fill_ms"),
                     "total_ms": med(g, "total_ms")}
summary["init"] = initc

# ---- MEM (malloc-family RSS) + PMAD exact -------------------------------
mem = {}
for be in ORDER[1:]:
    mem[be] = {}
    for s in SIZES:
        g = sel("MEM", backend=be, size=float(s))
        if g:
            mem[be][str(s)] = med(g, "overhead_pct")
summary["mem_rss"] = mem
# PMAD: exact deterministic header overhead (16 B header per block)
summary["mem_pmad_exact"] = {str(s): 100.0 * 16 / s for s in SIZES}

# ---- drift windows -------------------------------------------------------
wins = defaultdict(list)
wpath = os.path.join(RAW, "churn_windows.txt")
if os.path.exists(wpath):
    with open(wpath) as f:
        for line in f:
            t = line.split()
            if len(t) >= 8 and t[0] == "WIN":
                be, size, w = t[1], int(t[2]), int(t[3])
                p99 = float(t[6])
                wins[be].append((w, p99))
drift = {}
for be in ORDER:
    seq = [p for _, p in sorted(wins.get(be, []))]
    if seq:
        drift[be] = {"first": seq[0], "last": seq[-1], "max": max(seq),
                     "ratio": seq[-1] / seq[0] if seq[0] else None,
                     "series": seq}
summary["drift"] = drift

# ---- env -----------------------------------------------------------------
envp = os.path.join(RAW, "env.txt")
summary["env"] = open(envp).read().strip() if os.path.exists(envp) else ""

with open(os.path.join(RES, "summary.json"), "w") as f:
    json.dump(summary, f, indent=1)

# ---- markdown tables -----------------------------------------------------
def fmt(v, nd=2):
    return "—" if v is None else f"{v:.{nd}f}"

L = []
L.append("## Bench 1 — Hot-path latency by size (K=32 body), P50 / P99.9 ns/op\n")
L.append("| Allocator | " + " | ".join(f"{s}B" for s in SIZES) + " |")
L.append("|---" + "|--:" * len(SIZES) + "|")
for be in ORDER:
    cells = []
    for s in SIZES:
        e = lat.get(be, {}).get(str(s))
        cells.append(f"{fmt(e['p50'])} / {fmt(e['p99.9'])}" if e else "—")
    L.append(f"| {be} | " + " | ".join(cells) + " |")

L.append("\n## Bench 2 — Single-op tail (K=1), exceedance ppm (median of reps)\n")
for s in (64, 1024):
    L.append(f"\n@{s}B:\n")
    L.append("| Allocator | P99.9 | P99.99 | max | >=100ns ppm | >=1us ppm | >=10us ppm |")
    L.append("|---|--:|--:|--:|--:|--:|--:|")
    for be in ORDER:
        e = tail.get(be, {}).get(str(s))
        if e:
            L.append(f"| {be} | {fmt(e['p99.9'])} | {fmt(e['p99.99'])} | {fmt(e['max'],0)} | "
                     f"{fmt(e['ge100ns_ppm'],1)} | {fmt(e['ge1us_ppm'],2)} | {fmt(e['ge10us_ppm'],3)} |")

L.append("\n## Bench 3 — Throughput (uninstrumented), Mops/s\n")
L.append("| Allocator | " + " | ".join(f"{s}B" for s in SIZES) + " |")
L.append("|---" + "|--:" * len(SIZES) + "|")
for be in ORDER:
    L.append(f"| {be} | " + " | ".join(fmt(tput.get(be, {}).get(str(s)), 1) for s in SIZES) + " |")

L.append("\n## Bench 4 — Churn (working set 262144), body + tail\n")
for s in (64, 1024):
    L.append(f"\n@{s}B:\n")
    L.append("| Allocator | mean | P50 | P99 | P99.9 | >=100ns ppm | >=1us ppm | max ns |")
    L.append("|---|--:|--:|--:|--:|--:|--:|--:|")
    for be in ORDER:
        e = churn.get(be, {}).get(str(s), {})
        L.append(f"| {be} | {fmt(e.get('mean'))} | {fmt(e.get('p50'))} | {fmt(e.get('p99'))} | "
                 f"{fmt(e.get('p99.9'))} | {fmt(e.get('ge100ns_ppm'),1)} | "
                 f"{fmt(e.get('ge1us_ppm'),2)} | {fmt(e.get('max'),0)} |")

L.append("\n## Bench 5 — MIXED multi-class churn (7 weighted sizes 16..1024B)\n")
L.append("| Allocator | mean | P50 | P99 | P99.9 | >=100ns ppm | >=1us ppm | max ns |")
L.append("|---|--:|--:|--:|--:|--:|--:|--:|")
for be in ORDER:
    e = mixed.get(be, {})
    L.append(f"| {be} | {fmt(e.get('mean'))} | {fmt(e.get('p50'))} | {fmt(e.get('p99'))} | "
             f"{fmt(e.get('p99.9'))} | {fmt(e.get('ge100ns_ppm'),1)} | "
             f"{fmt(e.get('ge1us_ppm'),2)} | {fmt(e.get('max'),0)} |")

L.append("\n## Bench 6 — Free-order sensitivity @64B (alloc/free ns per op)\n")
L.append("| Allocator | LIFO a/f | FIFO a/f | RAND a/f |")
L.append("|---|--:|--:|--:|")
for be in ORDER:
    cells = []
    for pat in ("lifo", "fifo", "rand"):
        e = forder.get(be, {}).get(pat)
        cells.append(f"{fmt(e['alloc'])} / {fmt(e['free'])}" if e else "—")
    L.append(f"| {be} | " + " | ".join(cells) + " |")

L.append("\n## Bench 7 — Startup: init + first fill of 1M x 64B objects (ms)\n")
L.append("| Allocator | init ms | fill ms | total ms |")
L.append("|---|--:|--:|--:|")
for be in ORDER:
    e = initc.get(be)
    if e:
        L.append(f"| {be} | {fmt(e['init_ms'],3)} | {fmt(e['fill_ms'],3)} | {fmt(e['total_ms'],3)} |")

L.append("\n## Bench 8 — Drift over one long churn run @64B (per-window P99)\n")
L.append("| Allocator | first | last | max | last/first |")
L.append("|---|--:|--:|--:|--:|")
for be in ORDER:
    e = drift.get(be)
    if e:
        L.append(f"| {be} | {fmt(e['first'])} | {fmt(e['last'])} | {fmt(e['max'])} | {fmt(e['ratio'])}x |")

L.append("\n## Bench 9 — Memory overhead\n")
L.append("PMAD exact (16B header/block): " +
         ", ".join(f"{s}B={100.0*16/s:.1f}%" for s in SIZES))
L.append("\n| Allocator (RSS-measured) | " + " | ".join(f"{s}B" for s in SIZES) + " |")
L.append("|---" + "|--:" * len(SIZES) + "|")
for be in ORDER[1:]:
    L.append(f"| {be} | " + " | ".join(fmt(mem.get(be, {}).get(str(s)), 1) + "%"
             if mem.get(be, {}).get(str(s)) is not None else "—" for s in SIZES) + " |")

with open(os.path.join(RES, "tables.md"), "w") as f:
    f.write("\n".join(L) + "\n")

print("wrote", os.path.join(RES, "summary.json"))
print("wrote", os.path.join(RES, "tables.md"))
