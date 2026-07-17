# Security

## Reporting

Report issues to the repository.

## Current trust boundaries

- `libdense_sim` accepts in-process caller input and is not a network parser.
- DenseDB WAL and snapshot files must be protected by normal filesystem access
  controls. Their parsers reject checksum and version failures but should still
  be treated as security-sensitive file parsers.
- WATCH and fanout views are borrowed and must not be retained past their
  documented invalidation point.
- The Python extension is not declared safe for CPython free-threading mode.
