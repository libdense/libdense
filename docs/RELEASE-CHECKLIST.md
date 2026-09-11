# Public Release Checklist

## Artifacts

- [x] public C headers;
- [ ] versioned shared libraries and relative SONAME links;
- [ ] static libraries;
- [x] C++ wrapper header;
- [x] CPython 3.11 through 3.14 wheel sources and release matrix;
- [x] Linux x86-64, Linux ARM64, and Windows x86-64 target metadata;
- [ ] architecture-locked CMake package metadata in portable SDKs;
- [x] Python, C++, and Rust binding source;
- [x] API and ABI snapshots;
- [x] platform compatibility metadata;
- [x] version, changelog, security, and licensing notices;
- [x] install and uninstall scripts; and
- [x] generated SHA-256 checksums.

## Validation

- [ ] run `./verify-release.sh --metadata-only` before CI artifact assembly;
- [ ] run `./verify-release.sh`;
- [ ] run a staged install and complete uninstall;
- [ ] compile and run C consumers for all libraries present in each target;
- [ ] run the C++ wrapper tests;
- [ ] run the Rust wrapper tests;
- [ ] install each Python wheel in a clean matching interpreter and platform;
- [ ] confirm Windows artifacts omit DenseDB and server-only targets;
- [ ] confirm Linux ARM64 artifacts omit the client aggregate;
- [ ] confirm no core implementation source or Git history is present;
- [ ] create release archives preserving symlinks; and
- [ ] publish checksums independently of the archive.

## Licensing

Keep `LICENSE.md` unmodified. Binary redistribution by recipients remains
subject to its distribution conditions, including Section 8.
