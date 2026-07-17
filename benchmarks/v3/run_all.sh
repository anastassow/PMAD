#!/usr/bin/env bash
# run_all.sh (v3) — full benchmark suite. Emits raw data to raw/, from which
# every table and chart in the report is derived.
#
# All timed comparisons stay within PMAD's supported range (<=1024 B); the
# harness itself refuses out-of-range sizes rather than benchmarking a
# guaranteed-failure path (the v2 4096 B mistake).
set -euo pipefail
cd "$(dirname "$0")"

BACKENDS=(pmad system jemalloc tcmalloc mimalloc)
SIZES=(16 32 64 128 256 512 1024)
REPS="${REPS:-3}"            # reps for lat/tput/forder (median across reps)
TREPS="${TREPS:-5}"          # reps for K=1 tail runs (exceedance needs samples)
CREPS="${CREPS:-3}"          # reps for churn/mixed
LAT_ITERS="${LAT_ITERS:-16000000}"
TAIL_ITERS="${TAIL_ITERS:-20000000}"
CHURN_ITERS="${CHURN_ITERS:-32000000}"
DRIFT_ITERS="${DRIFT_ITERS:-64000000}"
CHURN_W="${CHURN_W:-262144}"
TPUT_BATCH="${TPUT_BATCH:-100000}"
TPUT_ROUNDS="${TPUT_ROUNDS:-200}"
FORD_COUNT="${FORD_COUNT:-1000000}"
FORD_ROUNDS="${FORD_ROUNDS:-10}"
INIT_COUNT="${INIT_COUNT:-1000000}"

mkdir -p raw results
RES=raw/all_results.txt
WIN=raw/churn_windows.txt
: > "$RES"; : > "$WIN"; : > raw/stderr.log

{ echo "date: $(date)"; echo "uname: $(uname -mrs)";
  echo "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo ?)";
  echo "cores: $(sysctl -n hw.ncpu 2>/dev/null || echo ?)";
  echo "jemalloc: $(brew list --versions jemalloc 2>/dev/null)";
  echo "mimalloc: $(brew list --versions mimalloc 2>/dev/null)";
  echo "gperftools(tcmalloc): $(brew list --versions gperftools 2>/dev/null)";
  echo "compiler: $(clang --version | head -1)";
  echo "git: $(git -C ../.. rev-parse --short HEAD 2>/dev/null)";
  echo "REPS=$REPS TREPS=$TREPS CREPS=$CREPS LAT_ITERS=$LAT_ITERS TAIL_ITERS=$TAIL_ITERS CHURN_ITERS=$CHURN_ITERS DRIFT_ITERS=$DRIFT_ITERS CHURN_W=$CHURN_W";
} | tee raw/env.txt

run(){ local be="$1"; shift; ./bench_"$be" "$@" 2>>raw/stderr.log; }

echo "### S1: memory overhead (RSS), malloc-family (PMAD's is exact/deterministic)" 1>&2
for be in system jemalloc tcmalloc mimalloc; do
  for s in "${SIZES[@]}"; do
    echo "rep=1 $(run "$be" mem "$s" 500000)" >> "$RES"
  done
done

echo "### S2: latency by size (K=32 body), reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    for s in "${SIZES[@]}"; do
      echo "rep=$rep $(run "$be" lat "$s" "$LAT_ITERS" 32)" >> "$RES"
    done
  done
  echo "  rep $rep/$REPS done (lat)" 1>&2
done

echo "### S3: single-op tail @64B & @1024B (K=1 exceedance), reps=$TREPS" 1>&2
for rep in $(seq 1 "$TREPS"); do
  for be in "${BACKENDS[@]}"; do
    for s in 64 1024; do
      echo "rep=$rep $(run "$be" lat "$s" "$TAIL_ITERS" 1)" >> "$RES"
    done
  done
  echo "  rep $rep/$TREPS done (tail)" 1>&2
done

echo "### S4: throughput by size, reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    for s in "${SIZES[@]}"; do
      echo "rep=$rep $(run "$be" tput "$s" "$TPUT_BATCH" "$TPUT_ROUNDS")" >> "$RES"
    done
  done
  echo "  rep $rep/$REPS done (tput)" 1>&2
done

echo "### S5: churn @64B & @1024B (K=32 body + K=1 tail), creps=$CREPS" 1>&2
for rep in $(seq 1 "$CREPS"); do
  seed=$((1000 + rep))
  for be in "${BACKENDS[@]}"; do
    for s in 64 1024; do
      echo "rep=$rep $(run "$be" churn "$s" "$CHURN_W" "$CHURN_ITERS" 32 "$seed")" >> "$RES"
      echo "rep=$rep $(run "$be" churn "$s" "$CHURN_W" "$TAIL_ITERS" 1 "$seed")" >> "$RES"
    done
  done
  echo "  crep $rep/$CREPS done (churn)" 1>&2
done

echo "### S6: MIXED multi-class churn (K=32 body + K=1 tail), creps=$CREPS" 1>&2
for rep in $(seq 1 "$CREPS"); do
  seed=$((2000 + rep))
  for be in "${BACKENDS[@]}"; do
    echo "rep=$rep $(run "$be" mixed "$CHURN_W" "$CHURN_ITERS" 32 "$seed")" >> "$RES"
    echo "rep=$rep $(run "$be" mixed "$CHURN_W" "$TAIL_ITERS" 1 "$seed")" >> "$RES"
  done
  echo "  crep $rep/$CREPS done (mixed)" 1>&2
done

echo "### S7: free-order sensitivity @64B, reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    for pat in lifo fifo rand; do
      echo "rep=$rep $(run "$be" forder 64 "$FORD_COUNT" "$pat" "$FORD_ROUNDS" 42)" >> "$RES"
    done
  done
  echo "  rep $rep/$REPS done (forder)" 1>&2
done

echo "### S8: startup cost (init + first full fill of ${INIT_COUNT} x 64B), reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    echo "rep=$rep $(run "$be" init 64 "$INIT_COUNT")" >> "$RES"
  done
done

echo "### S9: churn DRIFT @64B (one long run/backend, per-window trend)" 1>&2
for be in "${BACKENDS[@]}"; do
  ./bench_"$be" churn 64 "$CHURN_W" "$DRIFT_ITERS" 32 1001 2>>raw/win.tmp 1>>"$RES.drift" || true
  grep '^WIN ' raw/win.tmp >> "$WIN" || true
  : > raw/win.tmp
done
rm -f raw/win.tmp

echo "### ALL RUNS COMPLETE. raw -> $RES, windows -> $WIN" 1>&2
wc -l "$RES" "$WIN" 1>&2
