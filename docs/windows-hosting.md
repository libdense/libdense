# Windows Hosting

Windows x86-64 compatibility-server support is experimental. Windows binaries
are not included in this Linux x86-64 release. A separate MinGW-w64 static SDK
is required; this does not provide a native MSVC SDK or Windows DLL package.

## DenseDB checkpoints

The Windows backend preserves the public API and WAL/snapshot formats. It
flushes the WAL and temporary snapshot, replaces the snapshot, and retains the
WAL. It does not provide Linux's directory-fsync guarantee.

WAL disk use and recovery scan costs can therefore grow after checkpoints.
Monitor `ddb_durability_info.wal_bytes`. Do not manually delete or truncate the
WAL; back up the closed WAL and snapshot together. Online Windows WAL
compaction is not provided.

Checkpointing and database destruction require the single I/O consumer to be
stopped and joined, as on Linux. Use a local filesystem. Windows runtime and
power-loss durability are not validated by this Linux release's test results.
