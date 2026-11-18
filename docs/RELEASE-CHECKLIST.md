# Public Release Checklist

Release candidate 0.4.8, checked 2026-11-18. The final release repeats these
checks after the target-hardware run replaces the release-candidate figures.

This checklist applies to the Linux x86-64 release bundle. Other targets need
separate artifact validation.

## Artifacts

- [x] fifteen public C headers, including experimental `dense_gpu.h`;
- [x] four headless demo binaries with legacy defaults;
- [x] versioned shared libraries and relative SONAME links for nine libraries;
- [x] static libraries;
- [x] C++ wrapper header;
- [x] four Linux x86-64 CPython 3.11 through 3.14 wheels and binding source;
- [x] Python, C++, and Rust binding source;
- [x] API and ABI snapshots;
- [x] release-candidate plates of the 0.4.7 run;
- [x] platform compatibility metadata;
- [x] version, changelog, security, and licensing notices;
- [x] install and uninstall scripts; and
- [x] generated SHA-256 checksums.

## Validation

- [x] run `./verify-release.sh`;
- [x] run a staged install and complete uninstall;
- [x] compile and run C consumers for all libraries present, through
  pkg-config (shared and static) and the CMake package;
- [x] run the C++ wrapper tests;
- [x] run the Rust wrapper tests;
- [x] install each Python wheel in a clean matching interpreter and platform;
- [x] run each demo server and bot pair on loopback, every wire profile;
- [x] confirm the checkers reject a missing library, header drift, stray
  source, wrong exports and a stale wheel;
- [x] confirm no core implementation source or Git history is present;
- [x] create release archives preserving symlinks;
- [ ] run the target-hardware release benchmarks and replace the
  release-candidate figures;
- [ ] run the libdense_gpu OpenGL backend tests on the target GPU for this
  build; and
- [ ] publish checksums independently of the archive.

## Licensing

Keep `LICENSE.md` unmodified. Binary redistribution by recipients remains
subject to its distribution conditions, including Section 8.

Native ARM64/Windows and graphical clients are outside this validation.
