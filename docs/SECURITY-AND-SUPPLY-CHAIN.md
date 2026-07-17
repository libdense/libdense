# Security and Supply Chain

Run the release verifier and checksum validation before installation:

```bash
./verify-release.sh
sha256sum --check SHA256SUMS
```

The repository contains precompiled native code. Obtain release archives only
from the official publication location and compare hashes with the independently
published release checksums when available.

The installer supports `DESTDIR` staging and `--dry-run` so packagers can inspect
the exact filesystem changes before installation. The uninstaller uses an
installation manifest and refuses unsafe paths outside the selected prefix.

Security reports should follow `SECURITY.md`.
