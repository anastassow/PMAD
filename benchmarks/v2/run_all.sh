#!/usr/bin/env bash
# run_all.sh — full benchmark suite. Emits raw data to raw/, from which all
# tables in REPORT.md are derived (principle #12: emit raw, derive numbers).
#
# Reproducible: fixed per-rep seeds; environment captured to raw/env.txt.
set -euo pipefail
cd "$(dirname "$0")"

BACKENDS=(pmad system jemalloc tcmalloc mimalloc)
SIZES=(16 64 256 1024 4096)
REPS="${REPS:-5}"           # reps for latency/throughput (median across reps)
CREPS="${CREPS:-3}"         # reps for the long churn runs
LAT_ITERS="${LAT_ITERS:-16000000}"     # block-amortised body runs (K=32)
TAIL_ITERS="${TAIL_ITERS:-20000000}"   # single-op tail/exceedance runs (K=1)
CHURN_ITERS="${CHURN_ITERS:-64000000}" # long churn for drift
CHURN_W="${CHURN_W:-262144}"           # live working set (objects)
TPUT_BATCH="${TPUT_BATCH:-100000}"
TPUT_ROUNDS="${TPUT_ROUNDS:-200}"

mkdir -p raw results
RES=raw/all_results.txt
WIN=raw/churn_windows.txt
: > "$RES"; : > "$WIN"; : > "$RES.drift"

echo "### environment" | tee raw/env.txt
{ echo "date: $(date)"; echo "uname: $(uname -mrs)";
  echo "cpu: $(sysctl -n machdep.cpu.brand_string)";
  echo "cores: $(sysctl -n hw.ncpu) (perf=$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null||echo ?) eff=$(sysctl -n hw.perflevel1.physicalcpu 2>/dev/null||echo ?))";
  echo "jemalloc: $(brew list --versions jemalloc 2>/dev/null)";
  echo "mimalloc: $(brew list --versions mimalloc 2>/dev/null)";
  echo "gperftools(tcmalloc): $(brew list --versions gperftools 2>/dev/null)";
  echo "compiler: $(clang --version | head -1)";
  echo "REPS=$REPS CREPS=$CREPS LAT_ITERS=$LAT_ITERS TAIL_ITERS=$TAIL_ITERS CHURN_ITERS=$CHURN_ITERS CHURN_W=$CHURN_W";
} | tee -a raw/env.txt

run(){ # run <backend> <args...> ; tag stdout with rep, stderr -> WIN/header
  local be="$1"; shift
  ./bench_"$be" "$@" 2>>raw/stderr.log
}

echo "### Bench 5: memory overhead (RSS) — fast" 1>&2
: > raw/stderr.log
# PMAD deterministic header overhead
./pmad_mem > results/pmad_overhead.csv
# RSS-measured overhead for the malloc-family (PMAD pre-faults its pool, so RSS
# is not the right instrument for it — pmad_mem gives PMAD's exact number).
for be in system jemalloc tcmalloc mimalloc; do
  for s in "${SIZES[@]}"; do
    echo "rep=1 $(run "$be" mem "$s" 500000)" >> "$RES"
  done
done

echo "### Bench 3: latency-by-size (K=32 body) + throughput, reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    for s in "${SIZES[@]}"; do
      echo "rep=$rep $(run "$be" lat  "$s" "$LAT_ITERS" 32)"            >> "$RES"
      echo "rep=$rep $(run "$be" tput "$s" "$TPUT_BATCH" "$TPUT_ROUNDS")" >> "$RES"
    done
  done
  echo "  rep $rep/$REPS done (lat+tput)" 1>&2
done

echo "### Bench 1: headline tail @64B (K=1 single-op exceedance), reps=$REPS" 1>&2
for rep in $(seq 1 "$REPS"); do
  for be in "${BACKENDS[@]}"; do
    echo "rep=$rep $(run "$be" lat 64 "$TAIL_ITERS" 1)" >> "$RES"
  done
  echo "  rep $rep/$REPS done (tail)" 1>&2
done

echo "### Bench 4a: churn DRIFT @64B (one long run/backend, per-window trend)" 1>&2
for be in "${BACKENDS[@]}"; do
  ./bench_"$be" churn 64 "$CHURN_W" "$CHURN_ITERS" 32 1001 2>>raw/win.tmp 1>>"$RES.drift"
  grep '^WIN ' raw/win.tmp >> "$WIN" || true
  : > raw/win.tmp
done

echo "### Bench 4b: churn distribution @64B & @1024B (K=32 body + K=1 tail), creps=$CREPS" 1>&2
for rep in $(seq 1 "$CREPS"); do
  seed=$((1000 + rep))
  for be in "${BACKENDS[@]}"; do
    for s in 64 1024; do
      echo "rep=$rep $(run "$be" churn "$s" "$CHURN_W" "$CHURN_ITERS" 32 "$seed")" >> "$RES"
      echo "rep=$rep $(run "$be" churn "$s" "$CHURN_W" "$TAIL_ITERS" 1 "$seed")" >> "$RES"
    done
  done
  echo "  crep $rep/$CREPS done" 1>&2
done

echo "### ALL RUNS COMPLETE. raw -> $RES, windows -> $WIN" 1>&2
wc -l "$RES" "$WIN" 1>&2
