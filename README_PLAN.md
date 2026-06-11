# README Rewrite Plan — PMAD

A section-by-section blueprint for the new `README.md`. Built from
[`benchmarks/v2/REPORT.md`](benchmarks/v2/REPORT.md) (real, head-to-head,
reproducible numbers) and the quicx integration discussion.

> Delete this file once the README is rewritten.

---

## 0. Guiding principles (apply to every section)

1. **Honesty is the product.** The whole reason v2 exists is that a systems
   audience will scrutinise every number. Lead with *determinism*, not average
   speed. A defensible "we lose a few mid-tail percentiles to jemalloc/tcmalloc
   but guarantee the worst case" beats an indefensible "we beat everyone."
2. **Every number must be sourced and reproducible.** No web-cited competitor
   figures. Each headline number links to REPORT.md / `raw/`. State the machine.
3. **One claim per number, with its caveat attached.** e.g. "P50 = 2.59 ns
   (block-amortised, K=32, Apple M4 Pro)" — not a bare "2.59 ns".
4. **Show the tradeoffs on purpose.** A "When NOT to use" section and a memory
   overhead table build more trust than any benchmark.
5. **Keep the good bones.** The ASCII architecture diagram, repo structure, the
   configurator link, and the overall visual polish are worth keeping.

---

## 1. DELETE these from the current README (they break credibility)

| Current content | Why it must go | Replace with |
|---|---|---|
| "Sustained Latency **19.1 ns**" (everywhere) | v1 `CLOCK_MONOTONIC` artifact (1000 ns resolution — can't measure 20 ns). Real hot-path P50 is **2.59 ns**. | Real Bench 1/3 numbers. |
| "Jitter (σ) **0.0 ns (Strict)**" | False. Real P99.9 hot = 6.50 ns; churn max = 40 µs. Auditors will pounce. | "Flattest curve: P99.9/P50 = 2.5×; worst case bounded to ~40 µs under churn." |
| "PMAD vs Industry Giants" table with `~28.5 ns / ~480 M/s` etc. + "Sources: ithare.com, AppFolio…" | Not head-to-head; cherry-picked from the web; different machines. | The **real** head-to-head table from REPORT Bench 1. |
| "Throughput **>460 M/s**" as the headline | Understates it (real 16B = 749, 64B = 691 Mops) *and* hides the large-block cliff. | Honest per-size throughput (Bench 2). |
| "Fragmentation: 0%" | Misleading — PMAD has *fixed in-band header* overhead (50% at 16B). | "Zero *statistical* fragmentation; metadata overhead exact & predictable." |
| `pmad_init(classes, percentages)` in Usage + API | Wrong signature. Real: `pmad_init(class_sizes, num_size_classes, percentages, pool_size)` → returns `PmadStatus`. | Correct 4-arg signature (see §8). |
| "Optimal Performance Configurations" latency/throughput columns (19.1 ns…) | Same broken-timer numbers. | Keep the *recipes* (size classes + split); drop or re-measure the perf columns with the v2 harness. |

---

## 2. Proposed section order (the full skeleton)

```
1.  Hero (logo, title, honest tagline, badges)
2.  TL;DR — what PMAD is, in 4 lines + "at a glance"
3.  The headline table (real head-to-head) + the one killer churn number
4.  When to use / When NOT to use   ← trust-builder, put it early
5.  Benchmarks (summary of all 5, each links to REPORT.md)
6.  Real-world use case: quicx integration
7.  How it works (keep ASCII diagram + 4-step flow)
8.  Quick start (build → test → run → benchmark)
9.  Usage example + API reference (corrected signatures)
10. Configuration & tuning (configurator + honest preset recipes)
11. Design principles (de-hyped)
12. Limitations & honest tradeoffs   ← dedicated, not buried
13. Reproducibility ("every number traces to raw/")
14. Repository structure (add benchmarks/v2/)
15. Roadmap (thread-safety first, >4KB blocks)
16. Documentation / Contributing / License / footer
```

---

## 3. Section-by-section detail

### § Hero
- **Keep:** centered logo, title, badge row, visual style.
- **Fix tagline →** something honest and sharp, e.g.
  *"A deterministic, O(1) slab allocator with a provable latency ceiling —
  for systems where the worst case matters more than the average."*
- **Add badges:** `tests 19/19 passing`, `benchmarks reproducible`, keep C / platform / MIT / version. (Optional CI badge once you add a workflow.)

### § 2 — TL;DR
- 3–4 sentence elevator pitch (adapt REPORT §4 "the honest pitch"):
  O(1) alloc/free, one `mmap` at boot then **zero syscalls**, fixed footprint,
  latency **independent of block size**, worst case bounded by construction.
- "At a glance" bullets, all real:
  - Hot-path P50 **2.59 ns**, P99.9 **6.50 ns** (flattest body of any allocator tested)
  - **2.59 ns at every size 16→4096 B** — latency independent of block size
  - Under churn: PMAD max **40 µs** vs system allocator's **6.95 ms**
  - Throughput **691 Mops/s** @64B
  - **19/19** correctness checks pass
- End with: *"Every number below is measured head-to-head on one machine and
  reproducible — see [REPORT.md]."*

### § 3 — Headline table (THE hook)
- Use the **real** Bench 1 body table (PMAD / system / jemalloc / tcmalloc /
  mimalloc; mean, P50, P99, P99.9, P99.9/P50). Bold PMAD's flat ratio.
- Immediately under it, the single most dramatic line from Bench 4:
  > *Under sustained 1024 B churn, the system allocator's worst case blows out to
  > **6.95 ms** and mimalloc's to 73 µs — PMAD stays at **40 µs**. The slow path
  > that produces those spikes does not exist in PMAD's code.*
- Caption: "Apple M4 Pro, clang -O3, head-to-head same machine/day. Full method +
  raw data: [REPORT.md]."

### § 4 — When to use / When NOT to use
Two-column honesty table (straight from REPORT §4):

**Reach for PMAD when:**
- Objects are **small & fixed-size**, ≤ 4096 B, sizes known ahead of time
- You need a **provable latency ceiling** (RTOS, HFT, frame budgets, packet pools)
- **Single-threaded or shared-nothing per-core** ownership
- A fixed, capped memory footprint is acceptable / desirable

**Look elsewhere when:**
- You need a **general-purpose, multi-threaded shared heap** (use jemalloc/mimalloc)
- Allocations are **large or variable** (> 4 KB, unbounded payloads)
- You can't predeclare size classes
- You need **double-free / use-after-free detection** at runtime

### § 5 — Benchmarks (summary, link don't duplicate)
One short subsection per bench. Each = 1–2 sentences + the key table OR number,
then "→ full table, method, raw data in [REPORT.md]." Order:
1. **Tail-latency (headline)** — the flat curve; PMAD fewest ops ≥100ns (83 ppm).
2. **Latency by size (cleanest win)** — P50 = 2.59 ns at *every* size; the table.
3. **Churn / fragmentation (most important)** — give it the most space; the
   6.95 ms-vs-40 µs story + the drift sparkline showing flat lines. Be honest that
   at 64 B churn jemalloc/tcmalloc edge PMAD on ≥100 ns count.
4. **Throughput** — wins small (749/691), honest cliff ≥256 B (intrusive global
   free list, cache locality). Label it the *secondary* story.
5. **Memory overhead** — "choose your size": 50% @16B → 0.39% @4096B table; note
   PMAD faults the whole pool upfront (feature for RT, cost if over-provisioned).

### § 6 — Real-world use case: quicx
New, high-value credibility section.
- One paragraph: quicx = a high-performance C task-queue engine (epoll/kqueue,
  custom binary protocol, slab allocator, Java client), moving to a **shared-
  nothing multithreaded** dispatcher with an RL-based scheduler.
- Why PMAD fits, as bullets:
  - **Per-core shared-nothing** → one PMAD pool per worker, **no locks needed** —
    PMAD's single-threaded design becomes an asset, not a limitation.
  - **User-configured message length → maps directly to a PMAD size class** (PMAD
    is *built* around predeclared sizes).
  - **Bounded pool = natural backpressure**: exhaustion returns `NULL`, which a
    task queue should handle anyway; the RL dispatcher can use **per-core pool
    occupancy as a routing signal**.
  - Deterministic per-op latency → predictable queue SLAs.
- **State the caveat honestly:** PMAD caps blocks at 4096 B; for larger
  user-configured payloads raise `MAX_SIZE_OF_SIZE_CLASS` (cheap up to ~64 KB) or
  pair PMAD-for-small with a fallback for large.
- *(Only include if quicx is public / you're comfortable naming it. Otherwise
  retitle "Example integration: per-core task-queue buffer pool" and keep it
  generic.)*

### § 7 — How it works
- **Keep** the ASCII diagram + the 4-step Init/Alloc/Free/Destroy flow (they're
  good and accurate).
- Add one line that ties to the pitch: "Alloc = lookup-table index + free-list
  pop. Free = header read + free-list push. That's the entire worst case — there
  is no slow path to audit."
- Keep the Documentation_v1.0.pdf link.

### § 8 — Quick start
- Prereqs table: keep.
- **Add a `make test` step and surface "19/19 pass"** *before* the run step —
  mirrors REPORT principle #13 (correctness before speed).
- Update the benchmark commands to point at **`benchmarks/v2/`**
  (`make && make test && ./run_all.sh && python3 aggregate.py`), not the old
  single-file `benchmark.c`.

### § 9 — Usage + API reference (FIX SIGNATURES)
- Correct the example to the **real** API from [`include/incPMAD.h`](include/incPMAD.h):
  ```c
  size_t classes[]  = {16, 32, 64, 128, 256};
  size_t pcts[]     = {10, 20, 20, 20, 30};   // must sum to 100
  if (pmad_init(classes, 5, pcts, 1024*1024) != PMAD_OK) { /* handle */ }
  int* p = pmad_alloc(sizeof(int) * 4);       // O(1)
  pmad_free(p);                               // O(1)
  pmad_destroy();
  ```
- API table — correct signatures + add the two missing pieces:
  - `pmad_init(class_sizes, num_size_classes, percentages, pool_size) → PmadStatus`
  - `pmad_alloc(size) → void*` (returns `NULL` on exhaustion/oversize)
  - `pmad_free(ptr) → PmadStatus`
  - `pmad_destroy() → void`
  - `pmad_get_stats(out, max) → int`  ← currently undocumented
  - List the `PmadStatus` enum values (OK, OOM, INVALID_PTR, …).

### § 10 — Configuration & tuning
- Keep the "fully customizable" pitch and the **Live Pool Configurator** link
  (genuinely cool, mathematically exact — fine as-is).
- Keep the preset profiles **as configuration recipes** (size classes + split per
  domain), but **strip the 19.1 ns/throughput columns** or re-measure them with
  the v2 harness. Mark clearly which are re-measured vs illustrative.

### § 11 — Design principles
- Keep the five bullets but de-hype:
  - "Zero fragmentation" → "**No statistical fragmentation**; in-band header
    overhead is exact and predictable (see Bench 5)."
  - Drop "0.0 ns jitter." Keep "O(1) guaranteed — no fallback paths, no locking."
  - Keep single-mmap, user-defined layout, 16-byte header, no-dependencies.

### § 12 — Limitations & honest tradeoffs (NEW, dedicated)
Plain bullet list (from REPORT §4 losses) — do not bury these:
- **Single-threaded** (one global free list, no locks) — safe only per-thread /
  shared-nothing. *(Roadmap: per-thread pools.)*
- **Max block 4096 B** (`MAX_SIZE_OF_SIZE_CLASS`); larger needs a recompile or fallback.
- **No double-free / UAF detection** — a double `pmad_free` corrupts the free list.
- **Reserves & faults the whole pool upfront** — RSS = full pool from boot.
- **Throughput cliff** once the working set exceeds cache (large-block batches) —
  weaker spatial locality than sharded thread-cache allocators.
- **Not the most deterministic on small-block churn** — jemalloc/tcmalloc edge it
  on ≥100 ns op count at 64 B.

### § 13 — Reproducibility (NEW, short, powerful)
> "Every number in this README is reproducible. The benchmark harness is one
> source compiled once per allocator; raw samples are committed under
> [`benchmarks/v2/raw/`](benchmarks/v2/raw/); all tables derive from them."
```bash
cd benchmarks/v2 && make && make test && ./run_all.sh && python3 aggregate.py
```
Link [REPORT.md] for the 13-principle methodology.

### § 14 — Repository structure
- Keep the tree; **add the `benchmarks/v2/` subtree** (bench.c, pmad_test.c,
  pmad_mem.c, run_all.sh, aggregate.py, raw/, results/, REPORT.md). Mark the old
  `benchmark.c`/`bench_configs.c` as "legacy (v1)" or remove if superseded.

### § 15 — Roadmap
Reorder so the most credible/next item is first, and add the quicx-driven one:
- [ ] **Thread safety via per-thread / per-core pools** (enables shared-nothing
      engines like quicx) ← move to top
- [ ] **Blocks > 4 KB** (raise/replace the dense lookup table with bucket indexing)
- [ ] Optional debug build with double-free / UAF detection
- [ ] Dynamic pool expansion; per-class custom alignment; stats/monitoring API;
      RTOS integration examples (keep existing items)

### § 16 — Documentation / Contributing / License / footer
- Keep mostly as-is. Update the "Comparison with existing allocators" doc bullet
  to point at the new REPORT.md head-to-head. Keep MIT + footer.

---

## 4. Number swap-in cheat-sheet (old → real)

| Place | Old (delete) | New (use) | Source |
|---|---|---|---|
| Latency headline | 19.1 ns | P50 **2.59 ns** / P99.9 **6.50 ns** @64B | Bench 1 |
| Size independence | — | **2.59 ns @ 16→4096 B** | Bench 3 |
| Throughput | >460 M/s | **749/691** M/s @16/64B (cliff ≥256B) | Bench 2 |
| Jitter | 0.0 ns σ | P99.9/P50 = **2.5×**; max **40 µs** under churn | Bench 1, 4 |
| Determinism vs system | — | system max **6.95 ms** vs PMAD **40 µs** (1024B churn) | Bench 4 |
| Memory overhead | "0%" | **50% @16B → 0.39% @4096B** (exact) | Bench 5 |
| Competitor numbers | web-sourced | head-to-head, same machine | REPORT.md |
| Correctness | — | **19/19 pass** | §2 |

---

## 5. Tone checklist before publishing
- [ ] No number without a source/caveat and a machine named.
- [ ] Determinism leads; throughput is explicitly the secondary story.
- [ ] "When NOT to use" + "Limitations" are present and prominent.
- [ ] API signatures match `include/incPMAD.h` exactly.
- [ ] Every competitor figure is from your own head-to-head run, not the web.
- [ ] quicx section states the 4 KB caveat.
- [ ] A reader can reproduce every table with the commands given.
```
