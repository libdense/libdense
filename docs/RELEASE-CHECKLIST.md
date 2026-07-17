# Public Release Checklist

## Artifacts

- [x] public C headers;
- [x] versioned shared libraries and relative SONAME links;
- [x] static libraries;
- [x] C++ wrapper header;
- [x] CPython 3.13 and 3.14 wheels;
- [x] Python, C++, and Rust binding source;
- [x] API and ABI snapshots;
- [x] platform compatibility metadata;
- [x] version, changelog, security, and licensing notices;
- [x] install and uninstall scripts; and
- [x] generated SHA-256 checksums.

## Validation

- [ ] run `./verify-release.sh`;
- [ ] run a staged install and complete uninstall;
- [ ] compile and run C consumers for both libraries;
- [ ] run the C++ wrapper tests;
- [ ] run the Rust wrapper tests;
- [ ] install each Python wheel in a clean matching interpreter;
- [ ] confirm no core implementation source or Git history is present;
- [ ] create release archives preserving symlinks; and
- [ ] publish checksums independently of the archive.

## Licensing

Keep `LICENSE.md` unmodified. Binary redistribution by recipients remains
subject to its distribution conditions, including Section 8.
