## Bench 1 — Tail-latency distribution @ 64 B (headline)

Body percentiles (block-amortised, K=32; median across reps), ns/op:

| Allocator | mean | P50 | P90 | P99 | P99.9 |
|---|--:|--:|--:|--:|--:|
| PMAD | 2.31 | 2.59 | 2.62 | 3.91 | 6.50 |
| system | 16.74 | 15.62 | 16.94 | 20.84 | 239.59 |
| jemalloc | 7.69 | 7.81 | 7.81 | 16.91 | 144.53 |
| tcmalloc | 3.83 | 3.91 | 3.91 | 6.50 | 7.81 |
| mimalloc | 3.32 | 2.62 | 3.91 | 5.19 | 9.09 |

True single-op tail (K=1; tail percentiles quantised to 41 ns, exceedance counts exact), ns/op:

| Allocator | P99.9 | P99.99 | max | ≥100ns (ppm) | ≥1µs (ppm) | ≥10µs (ppm) |
|---|--:|--:|--:|--:|--:|--:|
| PMAD | 42.00 | 84.00 | 29250 | 83.3 | 49.15 | 1.850 |
| system | 125.00 | 459.00 | 50750 | 1686.5 | 94.55 | 3.050 |
| jemalloc | 84.00 | 167.00 | 34625 | 723.1 | 57.45 | 2.500 |
| tcmalloc | 42.00 | 125.00 | 33000 | 122.3 | 47.20 | 1.750 |
| mimalloc | 42.00 | 125.00 | 42833 | 106.4 | 42.40 | 1.250 |

## Bench 2 — Throughput (uninstrumented batch), Mops/s

| Allocator | 16B | 64B | 256B | 1024B | 4096B |
|---|--:|--:|--:|--:|--:|
| PMAD | 748.9 | 690.6 | 96.1 | 84.5 | 95.9 |
| system | 134.9 | 124.0 | 95.8 | 38.9 | 19.7 |
| jemalloc | 133.6 | 125.9 | 119.1 | 94.3 | 48.1 |
| tcmalloc | 267.8 | 211.8 | 111.7 | 93.8 | 37.5 |
| mimalloc | 467.9 | 408.1 | 297.3 | 84.6 | 30.4 |

## Bench 3 — Latency by block size (K=32 body), P50 / P99.9 ns/op

| Allocator | 16B | 64B | 256B | 1024B | 4096B |
|---|--:|--:|--:|--:|--:|
| PMAD | 2.59 / 6.50 | 2.59 / 6.50 | 2.59 / 6.50 | 2.59 / 5.19 | 2.59 / 6.50 |
| system | 15.62 / 222.66 | 15.62 / 239.59 | 15.62 / 239.59 | 19.53 / 237.00 | 14.34 / 239.56 |
| jemalloc | 7.81 / 52.06 | 7.81 / 144.53 | 7.81 / 143.22 | 7.81 / 154.94 | 9.09 / 15.62 |
| tcmalloc | 3.91 / 7.81 | 3.91 / 7.81 | 3.91 / 13.03 | 3.91 / 7.81 | 3.91 / 29.94 |
| mimalloc | 2.62 / 10.41 | 2.62 / 9.09 | 3.91 / 6.53 | 3.91 / 10.44 | 7.81 / 110.69 |

## Bench 4 — Churn @ 64 B, working set 262144 objects

Steady-state distribution (K=32 body; median across reps), ns/op:

| Allocator | mean | P50 | P90 | P99 | P99.9 |
|---|--:|--:|--:|--:|--:|
| PMAD | 8.68 | 6.53 | 14.31 | 22.12 | 111.97 |
| system | 54.58 | 49.47 | 80.72 | 151.06 | 354.16 |
| jemalloc | 8.53 | 7.81 | 9.12 | 11.72 | 181.00 |
| tcmalloc | 7.34 | 6.50 | 10.44 | 18.22 | 123.69 |
| mimalloc | 24.75 | 22.16 | 35.16 | 58.59 | 235.69 |

Single-op tail exceedance (K=1), fraction of ops over threshold:

| Allocator | ≥100ns (ppm) | ≥1µs (ppm) | ≥10µs (ppm) | max (ns) |
|---|--:|--:|--:|--:|
| PMAD | 17881.3 | 107.45 | 4.950 | 44292 |
| system | 242700.4 | 360.75 | 15.750 | 616875 |
| jemalloc | 816.1 | 53.00 | 1.900 | 27791 |
| tcmalloc | 1001.9 | 43.60 | 2.000 | 32416 |
| mimalloc | 44247.0 | 122.65 | 7.200 | 361458 |

## Bench 4 — Churn @ 1024 B, working set 262144 objects

Steady-state distribution (K=32 body; median across reps), ns/op:

| Allocator | mean | P50 | P90 | P99 | P99.9 |
|---|--:|--:|--:|--:|--:|
| PMAD | 14.11 | 13.03 | 18.22 | 33.84 | 230.47 |
| system | 209.39 | 200.50 | 251.31 | 513.03 | 707.03 |
| jemalloc | 10.77 | 10.41 | 11.72 | 15.62 | 147.16 |
| tcmalloc | 17.30 | 15.62 | 20.84 | 40.34 | 248.69 |
| mimalloc | 166.63 | 162.75 | 196.62 | 360.69 | 562.50 |

Single-op tail exceedance (K=1), fraction of ops over threshold:

| Allocator | ≥100ns (ppm) | ≥1µs (ppm) | ≥10µs (ppm) | max (ns) |
|---|--:|--:|--:|--:|
| PMAD | 29476.2 | 150.90 | 5.900 | 39875 |
| system | 951247.7 | 2208.65 | 44.900 | 6950250 |
| jemalloc | 957.8 | 52.70 | 2.150 | 34166 |
| tcmalloc | 15865.0 | 115.60 | 3.700 | 48833 |
| mimalloc | 824041.8 | 572.40 | 23.900 | 73375 |

### Bench 4 — Drift over a single long run @64B (per-window P99, ns/op)

| Allocator | first window P99 | last window P99 | windows-max P99 | drift (last/first) |
|---|--:|--:|--:|--:|
| PMAD | 31.25 | 9.12 | 44.25 | 0.29× |
| system | 268.25 | 37.78 | 283.84 | 0.14× |
| jemalloc | 9.12 | 11.72 | 28.66 | 1.29× |
| tcmalloc | 20.84 | 9.12 | 22.12 | 0.44× |
| mimalloc | 78.12 | 31.25 | 97.66 | 0.40× |

Per-window P99 across the long run (left=op 0 → right=op 64M), common scale 7.81→283.84 ns/op:

```
PMAD      ▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁  (last 9.12)
system    ▇▇▅▄▇▅▇▅▃▂▁▂▂▁▁▄▄▄▄▄▅▄▄▅▂▁▁▁▁▂▁▂▄▆▆▅▅▆▇▇█▂▁▁▁▁▁▁▃▇▆▇▇▇▇▇▆▄▁▁  (last 37.78)
jemalloc  ▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁  (last 11.72)
tcmalloc  ▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁  (last 9.12)
mimalloc  ▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▂▁▁▁▁▁▁▁▁▁▁▁▁▁▂▂▂▃▂▂▃▂▂▂▂▂▂▂▂▂▂▂▂▂▁▁▁▁▁▁▁  (last 31.25)
```

## Bench 5 — Memory overhead by block size

PMAD metadata overhead (exact, deterministic — 16-byte header):

| Size (B) | Header (B) | Blocks/64MB | Header overhead % |
|--:|--:|--:|--:|
| 16 | 16 | 2097150 | 50.0000 |
| 32 | 16 | 1398100 | 33.3333 |
| 64 | 16 | 838860 | 20.0000 |
| 128 | 16 | 466033 | 11.1111 |
| 256 | 16 | 246723 | 5.8824 |
| 512 | 16 | 127100 | 3.0303 |
| 1024 | 16 | 64527 | 1.5385 |
| 2048 | 16 | 32513 | 0.7752 |
| 4096 | 16 | 16320 | 0.3891 |

Measured RSS overhead, malloc-family (bytes resident per live object vs requested):

| Allocator | 16B | 64B | 256B | 1024B | 4096B |
|---|--:|--:|--:|--:|--:|
| system | 0.6% | 0.5% | 0.4% | 0.4% | 0.4% |
| jemalloc | 2.4% | 2.5% | 2.4% | 2.4% | 2.4% |
| tcmalloc | 0.3% | 0.3% | 0.3% | 0.3% | 0.5% |
| mimalloc | 0.1% | 0.2% | 0.6% | 0.2% | 0.2% |
