# 0.4.2 versus 0.4.4 Benchmarks

Public release: 2026-10-16. Measured on 2026-10-17 UTC. 0.4.3 was internal and
unpublished; its production changes are included in 0.4.4.

This comparison uses the exact static libraries from the supplied 0.4.2 release
repository and the freshly built, frozen 0.4.4 release defaults. No experimental
feature is enabled. The earlier internal comparisons and private shared-workset
results do not supply any number in this report.

## Selected results

At 4,096 distinct masks, full simulation ticks are 44.55x faster.
At 1,024 entities and recipients, mass workset departure is 39.92x
faster and no longer shifts the old 8,581,545,984 bytes. Complete shared-batch
encode/publish/flush at 1,024 frames and recipients is 11.94x faster.
These are separate workloads and timing boundaries, not an overall server multiplier.
With concurrent loopback UDP, the shared 512-entity pipeline falls from
31.96 to 19.98 ms through sender flush.

The controls are part of the result: the four-frame shared network mean changes
+16.0%, the 16-entity CPU crossing mean changes
+11.3%, the 512-entity CPU crossing mean changes
+8.3%, and the 512-entity concurrent UDP crossing mean changes
+5.2%. The 128-entity fragmented-mask UDP mean changes
+27.6%. Every measured condition, including slower means and tails,
appears below or in the accompanying data. Wire bytes match between releases
for every matched network workload.

| Workload | 0.4.2 mean (ms) | 0.4.4 mean (ms) | Time change | 0.4.2 p99 (ms) | 0.4.4 p99 (ms) |
|---|---:|---:|---:|---:|---:|
| Distinct masks / 1,024 | 1.281286 | 0.103175 | -91.9% | 1.525905 | 0.153687 |
| Distinct masks / 4,096 | 19.069729 | 0.428036 | -97.8% | 22.523990 | 0.561259 |
| Moving masks / 4,096 entities | 76.301238 | 1.578242 | -97.9% | 84.281482 | 1.906287 |
| Shared packet batch / 4 | 0.000442 | 0.000512 | +16.0% | 0.000451 | 0.000521 |
| Shared packet batch / 1,024 | 34.292668 | 2.872626 | -91.6% | 39.713610 | 3.670454 |
| CPU / shared / 512 | 11.848852 | 0.997330 | -91.6% | 14.258048 | 1.770647 |
| UDP / shared / 512 | 31.956672 | 19.984475 | -37.5% | 39.008313 | 24.514845 |
| CPU / crossings / 16 | 0.008672 | 0.009651 | +11.3% | 0.020140 | 0.022132 |
| CPU / crossings / 512 | 16.403423 | 17.768638 | +8.3% | 19.776158 | 22.060549 |
| UDP / crossings / 512 | 36.816582 | 38.720044 | +5.2% | 46.804017 | 47.014809 |
| UDP / fragmented masks / 128 | 0.608424 | 0.776535 | +27.6% | 0.853541 | 0.920350 |

Values are medians of five per-process means. A p99 is the median of five
independently calculated process p99 values, not a percentile pooled across
processes. Negative time changes mean less time. The summary and comparison CSV
also retain minimum and maximum process results for every numeric field.

## Inputs and coverage

| Input | SHA-256 |
|---|---|
| Supplied 0.4.2 release repository ZIP | `498560ff918f8b2a82deeed93ed0d528098f69dc91a998cae9eeccfdf03015e6` |
| Frozen development archive before release documentation | `9d51ad73fe54b3ecfa418f54f5d85fd9306500fef87f9c59b9ad0cd671547246` |

All 134 production C/header files remain byte-identical to the frozen 0.4.4
archive. Benchmark fixtures and documentation were added after the freeze.
The shipped 0.4.4 static libraries are the exact files linked by this comparison;
their hashes, the supplied 0.4.2 archive hashes, public headers and fixtures are
recorded in the [input manifests](../release/benchmarks/0.4.4/manifests/).

The host is Linux x86-64, AMD EPYC 9V74 80-Core Processor. Fixtures use GCC 13.3.0,
`-O3`, strict C11 warnings and `-ffp-contract=off`. 0.4.4 libraries use the ordinary
release build, without native tuning, LTO or experimental flags. The supplied
0.4.2 binaries identify GCC 13.3.0; their original command lines are not embedded
in the artifacts and were not reconstructed. This is a delivered-binary comparison.

Processes run serially, with the version order reversed on alternate repeats.
The sender is pinned to logical CPU 0. Four concurrent UDP receive workers share
logical CPU 1. Builds and test jobs finished before timing. These are controlled
measurements on one shared host, not a deployment-capacity guarantee.

| Suite | Conditions | Processes | Measured samples per process |
|---|---:|---:|---|
| Simulation and controls | 21 | 210 | 2,000 ticks; recreation 4,000; idle 10,000 |
| Explicit workset departure | 6 | 60 | One complete departure and one all-source enqueue |
| Shared network batch | 6 | 60 | 2,000 ticks; N = 1,024 uses 1,000 |
| Complete in-process pipeline | 8 | 80 | 2,000 ticks |
| Concurrent loopback UDP | 8 | 80 | 1,000 ticks |
| Total | 49 | 490 | 970,000 measured ticks plus 60 departure/enqueue pairs |

Steady-state fixtures use 32 warmup ticks. Cold/setup costs are separate fields.
All processes pass their exact output/counter oracles and allocation-balance
assertions. Output hashing is outside the timed interval. 460 of
490 processes report zero measured allocation growth; the per-case
counts are preserved, so this is not a universal zero-allocation claim.

## Complete simulation matrix

The timer includes begin-tick, mutations and finalization. Public fanout hashing
and logical-delivery counting occur afterward. Distinct-mask cases use one chunk;
moving-mask cases alternate entities between two chunks. Separate-chunk cases
exercise fragmented coverage. Shared all-to-all cases keep N entities and N
observers at fixed positions and mark every entity dirty, representing N squared
logical deliveries without expanding them. Recreation replaces one of 64 entities
per tick with 1,000 or 10,000 shared observers and marks all 64 entities dirty.
Idle controls have no dirty mutations.

| Workload | 0.4.2 mean (ms) | 0.4.4 mean (ms) | Time change | 0.4.2 p99 (ms) | 0.4.4 p99 (ms) |
|---|---:|---:|---:|---:|---:|
| Distinct masks / 1 | 0.000116 | 0.000108 | -7.2% | 0.000140 | 0.000130 |
| Distinct masks / 4 | 0.000396 | 0.000394 | -0.5% | 0.000431 | 0.000431 |
| Distinct masks / 16 | 0.001596 | 0.001395 | -12.6% | 0.001663 | 0.001402 |
| Distinct masks / 32 | 0.003757 | 0.002882 | -23.3% | 0.004346 | 0.002945 |
| Distinct masks / 64 | 0.009607 | 0.006153 | -36.0% | 0.022003 | 0.009845 |
| Distinct masks / 128 | 0.029606 | 0.012021 | -59.4% | 0.067440 | 0.026589 |
| Distinct masks / 256 | 0.099034 | 0.025243 | -74.5% | 0.163181 | 0.051326 |
| Distinct masks / 1,024 | 1.281286 | 0.103175 | -91.9% | 1.525905 | 0.153687 |
| Distinct masks / 4,096 | 19.069729 | 0.428036 | -97.8% | 22.523990 | 0.561259 |
| Moving masks / 1,024 entities | 5.084616 | 0.382147 | -92.5% | 5.533376 | 0.452318 |
| Moving masks / 4,096 entities | 76.301238 | 1.578242 | -97.9% | 84.281482 | 1.906287 |
| Separate chunks / 4,096 | 0.410190 | 0.377335 | -8.0% | 0.512768 | 0.467861 |
| Moving separate chunks / 4,096 | 1.332687 | 1.297995 | -2.6% | 1.519134 | 1.483672 |
| One shared class / 4,096 entities | 0.095909 | 0.094849 | -1.1% | 0.147148 | 0.151073 |
| All-to-all plan / 100 | 0.002449 | 0.002497 | +2.0% | 0.002474 | 0.002554 |
| All-to-all plan / 1,000 | 0.022416 | 0.022774 | +1.6% | 0.037245 | 0.037515 |
| All-to-all plan / 10,000 | 0.260679 | 0.257661 | -1.2% | 0.317880 | 0.323418 |
| Recreation / 64 entities, 1,000 observers | 0.002832 | 0.002714 | -4.2% | 0.004557 | 0.003095 |
| Recreation / 64 entities, 10,000 observers | 0.008313 | 0.008936 | +7.5% | 0.022384 | 0.025718 |
| Idle shared world / 100 | 0.000051 | 0.000053 | +3.9% | 0.000060 | 0.000080 |
| Idle shared world / 10,000 | 0.000376 | 0.000359 | -4.6% | 0.000370 | 0.000381 |

## Explicit worksets

Each process builds N squared memberships, enqueues all N dirty sources, removes
every entity, finalizes the world, then measures one incremental workset sync.
The enqueue and sync timers use process CPU time. Setup, entity removal and world
finalization are outside the sync timer. Five independent process results are
reported; one-operation trials do not establish a p99.
Both executables wrap `memmove` to count actual shifted bytes; that accounting
overhead is included in the sync timer.

| N | 0.4.2 departure (ms) | 0.4.4 departure (ms) | 0.4.2 enqueue (ms) | 0.4.4 enqueue (ms) | Shifted bytes: 0.4.2 / 0.4.4 |
|---:|---:|---:|---:|---:|---:|
| 32 | 0.016985 | 0.008422 | 0.026289 | 0.015413 | 253,952 / 0 |
| 128 | 0.497937 | 0.085838 | 0.476426 | 0.239585 | 16,646,144 / 0 |
| 256 | 3.484699 | 0.334285 | 2.455996 | 1.010756 | 133,693,440 / 0 |
| 512 | 26.831381 | 1.377079 | 10.820279 | 4.089735 | 1,071,644,672 / 0 |
| 1,024 | 226.283416 | 5.668051 | 64.116447 | 17.932007 | 8,581,545,984 / 0 |
| 2,048 | 4102.215603 | 22.721396 | 388.891079 | 76.963316 | 68,685,922,304 / 0 |

The old shift count is N x N x (N - 1) / 2 x sizeof(workset entry), with a
16-byte entry in this build. Bulk lifecycle merging removes that cubic shifting.
Explicit membership, dirty state and full per-recipient consumption still retain
quadratic costs. The private factored/shared-clean-view prototype is not installed.

## Shared network batch

The timer covers replication begin, encoding N fixed eight-byte payloads, publishing
to N sessions and flushing every session into a byte-capturing transport. The packet
limit is 1,400 bytes. Capture copies are timed; hashing is afterward. No kernel
transport is used in this suite. Exact packets, counts and logical frame references
match. Full stage costs and cold-tick time are in the data.

| Workload | 0.4.2 mean (ms) | 0.4.4 mean (ms) | Time change | 0.4.2 p99 (ms) | 0.4.4 p99 (ms) |
|---|---:|---:|---:|---:|---:|
| Shared packet batch / 1 | 0.000107 | 0.000119 | +10.9% | 0.000111 | 0.000111 |
| Shared packet batch / 4 | 0.000442 | 0.000512 | +16.0% | 0.000451 | 0.000521 |
| Shared packet batch / 16 | 0.004950 | 0.001091 | -78.0% | 0.005678 | 0.001182 |
| Shared packet batch / 128 | 0.302671 | 0.025430 | -91.6% | 0.366692 | 0.040680 |
| Shared packet batch / 512 | 7.808585 | 0.394122 | -95.0% | 9.702956 | 0.627458 |
| Shared packet batch / 1,024 | 34.292668 | 2.872626 | -91.6% | 39.713610 | 3.670454 |

Eligible shared-batch preparation is O(F + B + R), for frames F, eligible batches B
and recipient references R. Flushing independent records to every recipient still
emits quadratic data. This result does not claim reduced wire volume.

## Complete world-to-packet pipeline

The timer includes public world mutations, finalization, replication publication
and complete session flushing. Four origins alternate with a common destination.
Controls cover small worlds, one shared destination, existing direct writes,
variable-size payloads and fragmented masks. Payloads are 32 bytes, or 25-32 bytes
in the variable-size case. Packet limit is 1,400 bytes.

| Workload | 0.4.2 mean (ms) | 0.4.4 mean (ms) | Time change | 0.4.2 p99 (ms) | 0.4.4 p99 (ms) |
|---|---:|---:|---:|---:|---:|
| CPU / crossings / 4 | 0.002168 | 0.002177 | +0.4% | 0.002634 | 0.002563 |
| CPU / crossings / 16 | 0.008672 | 0.009651 | +11.3% | 0.020140 | 0.022132 |
| CPU / crossings / 128 | 0.365811 | 0.405365 | +10.8% | 0.471086 | 0.523693 |
| CPU / crossings / 512 | 16.403423 | 17.768638 | +8.3% | 19.776158 | 22.060549 |
| CPU / shared / 512 | 11.848852 | 0.997330 | -91.6% | 14.258048 | 1.770647 |
| CPU / occupied queues / 128 | 0.385236 | 0.431167 | +11.9% | 0.639424 | 0.634217 |
| CPU / variable payload / 128 | 0.370342 | 0.411016 | +11.0% | 0.498647 | 0.564173 |
| CPU / fragmented masks / 128 | 0.186771 | 0.210928 | +12.9% | 0.245973 | 0.284991 |

## Concurrent loopback UDP

The same pipeline sends real UDP datagrams to four concurrently running receivers.
The four workers share CPU 1; the sender uses CPU 0. A 32-packet credit window per
flow bounds outstanding work and avoids local drops. Every received datagram is
compared against the sender capture. The wall timer ends at sender flush; separate
fields report post-flush completion wait, send-syscall time, credit wait and polling.

| Workload | 0.4.2 mean (ms) | 0.4.4 mean (ms) | Time change | 0.4.2 p99 (ms) | 0.4.4 p99 (ms) |
|---|---:|---:|---:|---:|---:|
| UDP / crossings / 4 | 0.011180 | 0.011216 | +0.3% | 0.022954 | 0.023365 |
| UDP / crossings / 16 | 0.032511 | 0.033630 | +3.4% | 0.047320 | 0.047860 |
| UDP / crossings / 128 | 1.631536 | 1.770192 | +8.5% | 2.171117 | 2.625539 |
| UDP / crossings / 512 | 36.816582 | 38.720044 | +5.2% | 46.804017 | 47.014809 |
| UDP / shared / 512 | 31.956672 | 19.984475 | -37.5% | 39.008313 | 24.514845 |
| UDP / occupied queues / 128 | 1.656620 | 1.512200 | -8.7% | 2.079362 | 2.004661 |
| UDP / variable payload / 128 | 1.721056 | 1.410821 | -18.0% | 1.988528 | 1.855922 |
| UDP / fragmented masks / 128 | 0.608424 | 0.776535 | +27.6% | 0.853541 | 0.920350 |

| Workload | 0.4.2 sender CPU (ms) | 0.4.4 sender CPU (ms) | 0.4.2 receive CPU (ms) | 0.4.4 receive CPU (ms) | 0.4.2 / 0.4.4 completion wait (ms) |
|---|---:|---:|---:|---:|---:|
| UDP / crossings / 4 | 0.011443 | 0.011479 | 0.009738 | 0.009910 | 0.014050 / 0.013970 |
| UDP / crossings / 16 | 0.032806 | 0.033890 | 0.030717 | 0.032137 | 0.013308 / 0.014815 |
| UDP / crossings / 128 | 1.631309 | 1.770907 | 1.618878 | 1.762576 | 0.024506 / 0.022770 |
| UDP / crossings / 512 | 36.813466 | 38.718936 | 36.787800 | 38.681526 | 0.030504 / 0.031386 |
| UDP / shared / 512 | 31.951266 | 19.983441 | 31.933316 | 19.958860 | 0.031431 / 0.030304 |
| UDP / occupied queues / 128 | 1.657333 | 1.512983 | 1.643343 | 1.500576 | 0.025026 / 0.022353 |
| UDP / variable payload / 128 | 1.721689 | 1.411521 | 1.707407 | 1.397779 | 0.027783 / 0.021127 |
| UDP / fragmented masks / 128 | 0.609081 | 0.777269 | 0.598423 | 0.765464 | 0.018532 / 0.021885 |

Receive CPU is summed across workers and includes polling and copying. It excludes
application decoding and gameplay. The credit policy is part of this fixture;
these results do not represent unconstrained Internet traffic, loss recovery or a
complete client. The 1,000-tick UDP p99 estimates do not characterize rare stalls.

## Memory, growth and remaining costs

Tracked bytes count library allocation payloads and metadata, including retained
capacity. They exclude allocator headers, benchmark capture arrays and kernel
socket buffers. Pipeline peak totals sum module high-water counters and are an
upper bound rather than a simultaneous process peak. RSS is measured separately
and includes fixture buffers and thread overhead. Growth counts are successful
tracked growth operations, not page faults or all allocator calls.

| Workload | 0.4.2 retained (bytes) | 0.4.4 retained (bytes) | 0.4.2 peak (bytes) | 0.4.4 peak (bytes) | Growth: 0.4.2 / 0.4.4 |
|---|---:|---:|---:|---:|---:|
| Distinct masks / 1 | 65,564 | 67,140 | 65,564 | 67,140 | 0 / 0 |
| Distinct masks / 4 | 72,668 | 74,244 | 72,668 | 74,244 | 0 / 0 |
| Distinct masks / 16 | 72,668 | 74,476 | 72,668 | 74,476 | 0 / 0 |
| Distinct masks / 32 | 74,556 | 76,492 | 74,556 | 76,492 | 0 / 0 |
| Distinct masks / 64 | 78,332 | 80,524 | 78,332 | 80,524 | 0 / 0 |
| Distinct masks / 128 | 122,748 | 125,964 | 124,508 | 127,724 | 0 / 0 |
| Distinct masks / 256 | 211,580 | 216,844 | 215,004 | 220,268 | 0 / 0 |
| Distinct masks / 1,024 | 744,572 | 762,124 | 757,980 | 775,532 | 0 / 0 |
| Distinct masks / 4,096 | 2,876,540 | 2,943,244 | 2,929,884 | 2,996,588 | 0 / 0 |
| Moving masks / 1,024 entities | 1,137,888 | 1,171,928 | 1,140,032 | 1,174,072 | 0 / 0 |
| Moving masks / 4,096 entities | 4,449,504 | 4,581,848 | 4,457,792 | 4,590,136 | 0 / 0 |
| Separate chunks / 4,096 | 5,598,488 | 5,696,832 | 5,651,832 | 5,750,176 | 0 / 0 |
| Moving separate chunks / 4,096 | 6,270,296 | 6,434,176 | 6,323,640 | 6,487,520 | 0 / 0 |
| One shared class / 4,096 entities | 1,562,108 | 1,563,684 | 1,562,108 | 1,563,684 | 0 / 0 |
| All-to-all plan / 100 | 130,984 | 132,560 | 130,984 | 132,560 | 0 / 0 |
| All-to-all plan / 1,000 | 754,488 | 756,064 | 754,488 | 756,064 | 0 / 0 |
| All-to-all plan / 10,000 | 10,987,736 | 10,989,312 | 10,987,736 | 10,989,312 | 0 / 0 |
| Recreation / 64 entities, 1,000 observers | 415,064 | 416,640 | 415,064 | 416,640 | 0 / 0 |
| Recreation / 64 entities, 10,000 observers | 5,331,704 | 5,333,280 | 5,331,704 | 5,333,280 | 0 / 0 |
| Idle shared world / 100 | 119,944 | 121,520 | 119,944 | 121,520 | 0 / 0 |
| Idle shared world / 10,000 | 9,936,312 | 9,937,888 | 9,936,312 | 9,937,888 | 0 / 0 |
| Workset departure / 32 | 95,256 | 99,256 | 95,256 | 99,256 | 0 / 3 |
| Workset departure / 128 | 411,224 | 422,136 | 411,224 | 422,136 | 0 / 5 |
| Workset departure / 256 | 1,301,208 | 1,321,336 | 1,301,208 | 1,321,336 | 0 / 6 |
| Workset departure / 512 | 4,658,136 | 4,696,696 | 4,658,136 | 4,696,696 | 0 / 7 |
| Workset departure / 1,024 | 17,663,448 | 17,738,872 | 17,663,448 | 17,738,872 | 0 / 8 |
| Workset departure / 2,048 | 68,839,896 | 68,989,048 | 68,839,896 | 68,989,048 | 0 / 9 |
| Shared packet batch / 1 | 334,644 | 334,796 | 334,644 | 334,796 | 0 / 0 |
| Shared packet batch / 4 | 348,780 | 349,292 | 348,780 | 349,292 | 0 / 0 |
| Shared packet batch / 16 | 423,052 | 426,116 | 423,052 | 426,116 | 0 / 0 |
| Shared packet batch / 128 | 2,594,316 | 2,617,092 | 2,594,316 | 2,617,092 | 0 / 0 |
| Shared packet batch / 512 | 30,233,100 | 30,323,460 | 30,233,100 | 30,323,460 | 0 / 0 |
| Shared packet batch / 1,024 | 115,712,524 | 115,892,996 | 115,712,524 | 115,892,996 | 0 / 0 |
| CPU / crossings / 4 | 433,368 | 435,456 | 433,368 | 435,456 | 0 / 0 |
| CPU / crossings / 16 | 561,640 | 565,168 | 561,640 | 565,168 | 0 / 0 |
| CPU / crossings / 128 | 4,678,920 | 4,704,216 | 4,678,920 | 4,704,216 | 0 / 0 |
| CPU / crossings / 512 | 57,640,456 | 57,734,872 | 57,682,756 | 57,776,660 | 0 / 0 |
| CPU / shared / 512 | 57,610,440 | 57,714,664 | 57,695,044 | 57,798,756 | 0 / 0 |
| CPU / occupied queues / 128 | 4,699,400 | 4,719,144 | 4,699,400 | 4,719,144 | 0 / 0 |
| CPU / variable payload / 128 | 4,678,920 | 4,703,880 | 4,678,920 | 4,703,880 | 0 / 0 |
| CPU / fragmented masks / 128 | 4,747,464 | 4,765,288 | 4,747,464 | 4,765,288 | 0 / 0 |
| UDP / crossings / 4 | 433,880 | 435,968 | 433,880 | 435,968 | 0 / 0 |
| UDP / crossings / 16 | 562,152 | 565,680 | 562,152 | 565,680 | 0 / 0 |
| UDP / crossings / 128 | 4,679,432 | 4,704,728 | 4,679,432 | 4,704,728 | 0 / 0 |
| UDP / crossings / 512 | 57,640,968 | 57,735,384 | 57,683,268 | 57,777,172 | 0 / 0 |
| UDP / shared / 512 | 57,610,952 | 57,715,176 | 57,695,556 | 57,799,268 | 0 / 0 |
| UDP / occupied queues / 128 | 4,699,912 | 4,719,656 | 4,699,912 | 4,719,656 | 0 / 0 |
| UDP / variable payload / 128 | 4,679,432 | 4,704,392 | 4,679,432 | 4,704,392 | 0 / 0 |
| UDP / fragmented masks / 128 | 4,747,976 | 4,765,800 | 4,747,976 | 4,765,800 | 0 / 0 |

No density experiment is enabled by this release packaging pass. Type-mask indexes
and shared-batch metadata can retain additional memory. Ordinary queues retain
their historical capacity; bounded reclamation remains experimental. Explicit
worksets and independent N-to-N records retain quadratic storage or delivery costs.
Public API/ABI, visibility, update cadence, ordering, expiry, queue limits, explicit
prewarm reservations and wire contents are preserved.

## Plates and data

PNG files are ready to share; each has a matching editable SVG. Values are generated
from the checked summary, not transcribed from historical measurements.

| Plate | File |
|---:|---|
| 1 | [01-dense-0.4.2-vs-0.4.4-overview.png](../densebench/0.4.4/01-dense-0.4.2-vs-0.4.4-overview.png) / [SVG](../densebench/0.4.4/01-dense-0.4.2-vs-0.4.4-overview.svg) |
| 2 | [02-dense-type-mask-scaling.png](../densebench/0.4.4/02-dense-type-mask-scaling.png) / [SVG](../densebench/0.4.4/02-dense-type-mask-scaling.svg) |
| 3 | [03-dense-shared-simulation-controls.png](../densebench/0.4.4/03-dense-shared-simulation-controls.png) / [SVG](../densebench/0.4.4/03-dense-shared-simulation-controls.svg) |
| 4 | [04-dense-workset-departure.png](../densebench/0.4.4/04-dense-workset-departure.png) / [SVG](../densebench/0.4.4/04-dense-workset-departure.svg) |
| 5 | [05-dense-shared-network-preparation.png](../densebench/0.4.4/05-dense-shared-network-preparation.png) / [SVG](../densebench/0.4.4/05-dense-shared-network-preparation.svg) |
| 6 | [06-dense-complete-cpu-pipeline.png](../densebench/0.4.4/06-dense-complete-cpu-pipeline.png) / [SVG](../densebench/0.4.4/06-dense-complete-cpu-pipeline.svg) |
| 7 | [07-dense-concurrent-udp.png](../densebench/0.4.4/07-dense-concurrent-udp.png) / [SVG](../densebench/0.4.4/07-dense-concurrent-udp.svg) |
| 8 | [08-dense-peak-and-retained-memory.png](../densebench/0.4.4/08-dense-peak-and-retained-memory.png) / [SVG](../densebench/0.4.4/08-dense-peak-and-retained-memory.svg) |
| 9 | [09-dense-controls-and-tails.png](../densebench/0.4.4/09-dense-controls-and-tails.png) / [SVG](../densebench/0.4.4/09-dense-controls-and-tails.svg) |
| 10 | [10-dense-complete-simulation-matrix.png](../densebench/0.4.4/10-dense-complete-simulation-matrix.png) / [SVG](../densebench/0.4.4/10-dense-complete-simulation-matrix.svg) |
| 11 | [11-dense-complete-delivery-matrix.png](../densebench/0.4.4/11-dense-complete-delivery-matrix.png) / [SVG](../densebench/0.4.4/11-dense-complete-delivery-matrix.svg) |

- [All measured runs](../release/benchmarks/0.4.4/samples.jsonl)
- [Checked summary with process ranges](../release/benchmarks/0.4.4/summary.json)
- [Complete comparison CSV](../release/benchmarks/0.4.4/comparison.csv)
- [Environment, matrix and build identities](../release/benchmarks/0.4.4/environment.json)
- [Input freeze](../release/benchmarks/0.4.4/source-freeze.json)

The release repository contains measurements, public interfaces, compiled libraries
and wrapper source. Core implementation and benchmark C source remain excluded,
following the existing release layout. The development archive retains the
public-API benchmark fixtures, runner and plate-generation scripts for reproduction.
Historical 0.3.8/0.4.2 results remain in their original directories and are not mixed
with this matrix.
