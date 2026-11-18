# Dense v0.4.7 plates (0.4.8 release candidate figures)

Ten PNG plates (2500x1625) and a vector PDF in the Dense cream/ink style.
They show the 0.4.7 run of the library sources that 0.4.8 carries unchanged
apart from the version macros. The final 0.4.8 release replaces them with a
run of 0.4.5 against 0.4.8 on the same machine.

All figures come from the owner's Ryzen 5 5600X runs on 2026-11-12, one
pinned logical CPU. The plates were rendered only from evidence in which
every packet stream was identical, M6 outputs were identical, Mithsteel never
fell back to grouped encoding, and density bytes were equal between
versions.

| Plate | Content |
| --- | --- |
| 01 | Density matrix, v0.4.7: simulation and the four sender modes, 1,000 to 25,000 players |
| 02 | Change from v0.4.5, same session and harness |
| 03 | Traffic saving per mode and retained sender memory |
| 04 | Complete server tick, v0.4.6.18 (the v0.4.5 sender with the v0.4.6 corrections) against v0.4.7 |
| 05 | 200-tick tails and the controls (A/A, v0.4.6.20 against v0.4.7) |
| 06 | M6 simulation benchmarks, v0.4.5 against v0.4.7 |
| 07 | Costs: collision endpoint guard, retained memory, allocation growth |
| 08 | Lean models and how they are connected to the C |
| 09 | Defaults, opt-ins and the segmented UDP experiment |
| 10 | Platforms and how each was checked |

The sender figures use a transport sink, not a network. No server capacity is
inferred. See the [0.4.8 release notes](../../docs/RELEASE-NOTES-0.4.8.md)
for the same results as tables.
