#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT="$ROOT_DIR/SHA256SUMS"
TMP_FILE="$(mktemp)"
trap 'rm -f -- "$TMP_FILE"' EXIT

cd "$ROOT_DIR"

while IFS= read -r -d "" path; do
    relative="${path#./}"
    sha256sum -- "$relative"
done < <(
    find . -type f \
        ! -path "./.git/*" \
        ! -path "./stage/*" \
        ! -path "./package/*" \
        ! -name "SHA256SUMS" \
        -print0 | sort -z
) > "$TMP_FILE"

mv -- "$TMP_FILE" "$OUTPUT"
trap - EXIT
printf "Wrote %s\n" "$OUTPUT"
