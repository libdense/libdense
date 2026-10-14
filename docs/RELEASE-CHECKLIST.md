# Public Release Checklist

This checklist applies to the Linux x86-64 release bundle. Other targets need
separate artifact validation.

## Artifacts

- [x] public C headers;
- [x] versioned shared libraries and relative SONAME links;
- [x] static libraries;
- [x] C++ wrapper header;
- [x] four Linux x86-64 CPython 3.11 through 3.14 wheels and binding source;
- [x] Python, C++, and Rust binding source;
- [x] API and ABI snapshots;
- [x] platform compatibility metadata;
- [x] version, changelog, security, and licensing notices;
- [x] install and uninstall scripts; and
- [x] generated SHA-256 checksums.

## Validation

- [x] run `./verify-release.sh`;
- [x] run a staged install and complete uninstall;
- [x] compile and run C consumers for all libraries present in each target;
- [x] run the C++ wrapper tests;
- [x] run the Rust wrapper tests;
- [x] install each Python wheel in a clean matching interpreter and platform;
- [x] confirm no core implementation source or Git history is present;
- [x] create release archives preserving symlinks; and
- [ ] publish checksums independently of the archive.

## Licensing

Keep `LICENSE.md` unmodified. Binary redistribution by recipients remains
subject to its distribution conditions, including Section 8.
