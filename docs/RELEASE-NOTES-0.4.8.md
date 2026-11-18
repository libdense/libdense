# Dense 0.4.8 release notes

Status: release candidate. The measurements below come from the 0.4.7 run
(2026-11-12) of the same library sources. Before the final release, a run on
the target hardware replaces them with 0.4.5 against 0.4.8, measured in one
session.

Component versions: libdense_sim 0.4.8, the delivery worker SDK 0.4.8,
libdense_net 0.3.5, libdense_sched, libdense_collision, libdense_nav,
libdense_ai and DenseDB 0.3.0; experimental libdense_gpu 0.1.7. Public API,
ABI (SONAME major 0) and wire format are unchanged from 0.4.5.

0.4.7 was cut on 2026-11-12 and not published. 0.4.8 carries its library
unchanged apart from the version macros, so these notes cover everything
since 0.4.5.

## Faster complete ticks

Ryzen 5 5600X, one pinned logical CPU, packets byte-identical in every
scene. 0.4.6.18 is the last development iteration before the sender work;
its sender is the 0.4.5 sender with the 0.4.6 corrections.

| Scene, milliseconds per tick | 0.4.6.18 | 0.4.7 sources |
| --- | ---: | ---: |
| 2,000 clients over a 64x64 world | 298.38 | 6.73 |
| 2,000 clients over 256x256 | 53.39 | 6.27 |
| 2,000 clients over 64x64, reliable ENTER/LEAVE | 302.04 | 7.50 |
| 2,000 clients over 256x256, reliable ENTER/LEAVE | 52.27 | 6.74 |
| 20,000 clients, sparse world | 68.72 | 51.70 |
| 2,000 clients over 64x64, one direct message each after publish | 305.24 | 9.01 |

The gains come from a session lane that queues any number of shared
replication batches per tick, a flush path that bulk-copies queued batches,
an exact tail for direct sequenced sends behind queued batches, and skipping
the retransmit scan while no reliable packet is retained.

The simulation is faster too: the five M6 benchmarks are 26.8% faster than
0.4.5 (17.5% to 41.1%, outputs identical). In the density matrix (1,000 to
25,000 entities, same session as 0.4.5), Mithsteel sender CPU is 50% to 58%
lower, Grouped 32% to 41%, Compact 30% to 50% and simulation 16% to 17%.
Legacy is within run-to-run variation at 1,000 and 25,000 entities and 4% to
10% lower at 5,000 and 10,000.

The plates in [densebench/0.4.7](../densebench/0.4.7/README.md) show these
runs. The sender figures use a transport sink, not a network; no server
capacity is inferred from them.

## Costs

- The collision endpoint guard adds 0.4% to 32.5% per move; the largest cost
  is in a 120-body crowd (738 to 978 ns). It is a correctness fix (below).
- Retained network memory is 0.1 to 17.2 MB higher in scenes without
  reliable traffic (sparse 20,000 clients: +0.9%) and lower with it (2,000
  clients, reliable: 1,790 to 1,503 MB).
- Allocation growth after warmup rises in six of twelve measured scenes (up
  to 12,709 growth operations per run in the sparse scene) and falls in five.

## Corrections that change behavior since 0.4.5

- **Collision.** Movement candidates outside the coordinate limits are
  rejected before commit. Circle and corner contact times are the exact Q16
  floor. A move whose final position would penetrate an obstacle is
  cancelled to the original position; touching is allowed, and initial
  overlaps are not resolved automatically. Contact times, normals and
  selected contacts can differ from 0.4.5, so **recorded replays that
  involve collisions can produce different results**.
- **Token bucket.** Refill ceiling and deadline wraparound are corrected.
- **Frames.** The legacy frame-size check no longer wraps on 32-bit targets.
- **Compact transport.** An oversized datagram no longer stalls compact
  loopback receive; a refused compact bootstrap reply no longer fails the
  endpoint's receive poll.
- **Grouped state and Mithsteel.** A baseline outside the predictor horizon
  no longer consumes prediction budget. Packet bytes are unchanged.
- **Loopback links.** `dn_loopback_link_create` returns `DN_ERR_CAPACITY`
  when the queue size does not fit in `size_t`.
- **Sorting.** A guard closes a theoretical ordering finding in the u64 sort.

The public headers document these contracts; their declarations and
structure layouts are those of 0.4.5. The replay library fingerprint names
the family version, as in every release: a replay recorded by 0.4.5 reports
a fingerprint mismatch when it is verified against the 0.4.8 library's
fingerprint.

## libdense_gpu 0.1.7 (experimental, opt-in)

Exact bulk spatial queries for local simulations: grid binning, k nearest
neighbors within a radius, in-radius aggregates and per-cell aggregate
fields over `int32` points in 2D or 3D. The CPU backend is the reference and
the default. The OpenGL 4.3 compute backend runs on the application's own
context and must return the same bytes; the library links neither OpenGL nor
SDL. API and ABI may change. It ships here as `include/dense/dense_gpu.h`,
`libdense_gpu.so.0.1.7`, `libdense_gpu.a` and a pkg-config file, and it is
outside the bindings, the CMake package and the frozen API baselines. See
[the module notes](modules/libdense_gpu.md).

Measured with 0.1.6 (the same kernels) on the Ryzen 5 5600X with an RTX 3070
(NVIDIA 535.261.03), every GPU result byte-identical to the CPU, whole
benchmark tick, flock scene, each arm in a run of its own, p50 / p99 ms:

| Points | CPU, 1 thread | CPU, aggregate on 12 threads | GPU |
| ---: | ---: | ---: | ---: |
| 100,000 | 97.6 / 136.4 | 30.6 / 47.6 | 4.0 / 4.9 |
| 1,000,000 | 1,029.0 / 1,562.6 | 227.1 / 333.0 | 27.9 / 41.5 |
| 10,000,000 | 14,655.3 / 15,701.5 | 3,357.0 / 3,711.1 | 583.9 / 618.9 |

With the entities kept in cell order by `dg_context_cell_order` every 8 to
32 ticks, the GPU tick at 10,000,000 points is 242 to 261 ms with the cost
of reordering counted. A GPU left idle for 200 ms or more runs its next tick
two to six times slower.

0.1.7 fixes a latent defect found in the release review: no buffer may
exceed 2^34 bytes, the most the shaders' uint indices address, whatever
larger limit a driver reports. No measured context reports more than 2^31
bytes.

Differences at 10,000,000 points in three development runs on the test
machine were single bits of host memory, in the CPU's arrays as often as in
the GPU's; the machine's memory test found a failing cell. Neither backend
computed a wrong value.

## Verification

Optional Lean 4 models in the implementation repository cover Time Bending /
Time Dilation, the collision arithmetic, the u64 sorts and class ranking,
frame decoding, the compact handshake, the grouped codec, token-bucket
accounting, the shared batch queue and the loopback allocation size (3,036
declarations, standard axioms only). They are connected to the C by
differential tests, decision-trace replay and mutation testing; the C
library itself is not formally verified. Nothing in this binary release
needs Lean.

## Platforms

Linux x86-64 is qualified on the target hardware and is the only target
with binaries in this release. Linux i386 runs the test suites natively and
Linux aarch64 under emulation (qemu); Windows x86-64 builds and links.
Native ARM64 and Windows execution are not qualified. libdense_gpu's GPU
backend is qualified on one machine (RTX 3070, NVIDIA 535.261.03) and
checked on Mesa llvmpipe.

## Opt-in and experimental

Legacy framing and single-threaded operation remain the defaults. Compact
framing, grouped state, two-byte Mithsteel, the delivery worker SDK, Time
Bending / Time Dilation and libdense_gpu are opt-in. Segmented UDP
submission is a default-off compile-time experiment of the implementation
build; the libraries in this release are built without it.

## This binary release

- Linux x86-64 shared and static libraries for the eight SDK libraries and
  experimental libdense_gpu, fifteen public C headers, pkg-config files and
  the CMake package.
- Headless Arena and Driving demonstrations rebuilt against 0.4.8.
- CPython 3.11 to 3.14 wheels of the `dense_sim` binding, version 0.4.8,
  and the C++ and Rust wrapper source.
- Versioned libraries and wheels of earlier releases are no longer kept in
  the repository tree; earlier tags and the Git history keep them.
- Video production workflows (0.1.0) are part of the implementation
  repository and are not in this binary release.

See [platform compatibility](PLATFORM-COMPATIBILITY.md) and
[release validation](../release/validation/0.4.8-rc.json).
