# libdense v0.4.5 release candidate

The v0.4.5 SDK opt-ins are implemented and packaged: compact framing, grouped
state, two-byte Mithsteel, local delivery workers and Time Bending / Time Dilation.
Legacy framing and ordinary single-threaded operation remain defaults. No feature
is activated simply by installing this SDK.

- [Grouped state and Mithsteel](grouped-state.md): public codec with exact
  corrections and bounded prediction work for the documented 48-byte schema.
- [Delivery workers](delivery-workers.md): separate static/shared Linux SDK,
  explicit session ownership, bounded publications and safe partial-work resume.
- Fix stale-publication admission after failed simulation capture. Idle capture
  failures invalidate prior sealed state while active worker ownership is preserved.
- [Time Bending / Time Dilation](time-dilation.md): fixed simulation steps,
  integer scales, epoch/tick controls, bounded ramps, exact snapshot restore and
  explicit backlog recovery. Network maintenance continues on real time.
- Installed headers, pkg-config/CMake integration, API/exports, executable script
  modes and source/public packaging are qualified together.

This candidate is qualified on Linux x86-64 in the recorded environment.
[Validation and platform limits](../release/validation/0.4.5-time-sdk.json).
[Bug review and regression checks](../release/validation/0.4.5-bug-review.json).
It has not been published or tagged. Other native platforms require their own
runtime qualification. Experimental Oracle, one-byte Mithsteel, relays and
unpromoted sender optimizations are excluded from the supported feature claims.
Historical benchmarks retain their original scope; these changes do not establish
new hardware performance or equivalent real-time capacity under dilation.
