# Dense Architecture

Dense 0.2 is a single repository containing separately versioned C modules. The root orchestrates builds and policy; it does not erase module boundaries.

## Core invariants

1. `libdense_sched` assigns every overload-sensitive system to a named phase and budget.
2. `libdense_collision` validates proposed movement.
3. `libdense_sim` commits validated movement and owns entity membership, dirty state, lifecycle, and canonical fanout.
4. `libdense_net` transports replication from fanout views and never recreates visibility grouping.
5. `libdense_nav` proposes movement under a global path-work budget.
6. `libdense_ai` emits intents only.
7. DenseDB receives writes through an explicit single-writer flush seam.
8. Runtime CPU dispatch is initialized once before worker threads.
9. Prewarmed representative tick paths should perform no allocations.

## Tick ordering

```text
input -> validate -> spatial -> combat -> AI -> fanout -> flush
```

A typical movement path is:

```text
input proposal
  -> navigation proposal when needed
  -> collision validation
  -> simulation commit
  -> chunk membership and dirty-state update
  -> fanout generation
  -> network transport
```

## Shared internals

`dense_core` is the private static implementation archive for shared maps, dirty sets, retained arenas, generation slot pools, canonical span hashing, CPU feature dispatch, and hybrid integer sorting. Its headers and archive are not installed. Defined symbols use hidden visibility, root ABI checks reject exports matching symbols defined by the private archive, and release LTO may inline private calls across archive boundaries. Existing module storage remains unchanged until Phase 3 representative benchmarks justify each conversion.
