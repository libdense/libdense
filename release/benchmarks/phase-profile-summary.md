# Phase 7 Integrated Tick Profile

Ticks: **240**

## Tick latency

| Metric | Time |
|---|---:|
| p50 | 14.337 us |
| p95 | 20.870 us |
| p99 | 56.707 us |
| max | 248.096 us |

## Per-phase latency

| Phase | p50 us | p95 us | p99 us | max us | completed | deferred |
|---|---:|---:|---:|---:|---:|---:|
| input | 0.030 | 0.031 | 0.531 | 2.615 | 9 | 0 |
| validate | 1.684 | 3.797 | 8.095 | 17.102 | 570 | 0 |
| spatial | 0.070 | 0.101 | 0.130 | 0.140 | 0 | 0 |
| combat | 0.020 | 0.711 | 0.781 | 1.012 | 24 | 0 |
| AI | 0.150 | 0.300 | 0.592 | 227.898 | 570 | 2310 |
| fanout | 0.842 | 2.604 | 3.226 | 14.076 | 798 | 0 |
| flush | 9.317 | 13.605 | 19.176 | 29.486 | 60875 | 151 |

## Operational answer

The phase most often consuming the tick was **flush**.
The slowest tick was **199** at **248.096 us**, dominated by **ai**.

Busiest-phase counts:

- `ai`: 2
- `flush`: 238

## Aggregate work

- Fanout deltas: 798
- Network bytes sent: 21384
- DenseDB flush bytes: 39491
- Final WAL bytes: 39523
- Final retained bytes: 3837201
- Peak retained bytes: 3853897
- Profile-window library allocations: 67
- Allocation failures: 0
