#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ERRORS=0
WARNINGS=0

error() {
    printf "ERROR: %s\n" "$*" >&2
    ((ERRORS += 1))
}

warn() {
    printf "WARN:  %s\n" "$*" >&2
    ((WARNINGS += 1))
}

require_file() {
    [[ -f "$ROOT_DIR/$1" ]] || error "missing required file: $1"
}

for relative in \
    README.md \
    LICENSE.md \
    COMMERCIAL-LICENSE.md \
    SECURITY.md \
    CHANGELOG.md \
    VERSION \
    MANIFEST.md \
    Makefile \
    install.sh \
    uninstall.sh \
    verify-release.sh \
    tools/generate-checksums.sh \
    SHA256SUMS \
    include/dense/dense_sim.h \
    include/dense/densedb.h \
    bindings/cpp/include/dense/dense_sim.hpp \
    release/abi/libdense_sim.exports \
    release/abi/libdensedb.exports; do
    require_file "$relative"
done

for script in install.sh uninstall.sh verify-release.sh tools/generate-checksums.sh; do
    if [[ -f "$ROOT_DIR/$script" ]]; then
        bash -n "$ROOT_DIR/$script" || error "bash syntax failed: $script"
        [[ -x "$ROOT_DIR/$script" ]] || error "script is not executable: $script"
    fi
done

VERSION=""
if [[ -f "$ROOT_DIR/VERSION" ]]; then
    VERSION="$(tr -d '[:space:]' < "$ROOT_DIR/VERSION")"
    [[ -n "$VERSION" ]] || error "VERSION is empty"
    [[ "$VERSION" != *" "* ]] || error "VERSION contains whitespace"
fi

for forbidden in \
    .git \
    libdense_sim \
    densedb \
    src \
    tests \
    benchmarks \
    private; do
    [[ ! -e "$ROOT_DIR/$forbidden" ]] || error "forbidden core/private path exists: $forbidden"
done

while IFS= read -r -d "" source_file; do
    relative="${source_file#"$ROOT_DIR/"}"
    case "$relative" in
        bindings/*)
            ;;
        *)
            error "implementation-like source exists outside bindings: $relative"
            ;;
    esac
done < <(
    find "$ROOT_DIR" -type f \
        \( -name "*.c" -o -name "*.cc" -o -name "*.cpp" -o -name "*.cxx" \) \
        -print0
)

while IFS= read -r -d "" generated; do
    error "unexpected generated/private build file: ${generated#"$ROOT_DIR/"}"
done < <(
    find "$ROOT_DIR" -type f \
        \( -name "*.o" -o -name "*.d" -o -name "*.pyc" -o -name "*.pyo" \) \
        -print0
)

while IFS= read -r -d "" image; do
    error "image artifact is present: ${image#"$ROOT_DIR/"}"
done < <(
    find "$ROOT_DIR" -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.gif" -o -iname "*.webp" -o -iname "*.svg" \) \
        -print0
)

LIB_DIR="$ROOT_DIR/lib/linux-x86_64"
[[ -d "$LIB_DIR" ]] || error "missing native artifact directory: lib/linux-x86_64"

check_symlink() {
    local path="$1"
    local expected="$2"
    if [[ ! -L "$path" ]]; then
        error "expected symlink: ${path#"$ROOT_DIR/"}"
        return
    fi
    local actual
    actual="$(readlink -- "$path")"
    [[ "$actual" == "$expected" ]] || error "unexpected symlink target: ${path#"$ROOT_DIR/"} -> $actual"
    [[ -e "$(dirname -- "$path")/$actual" ]] || error "broken symlink: ${path#"$ROOT_DIR/"}"
}

check_symlink "$LIB_DIR/libdense_sim.so" "libdense_sim.so.0"
check_symlink "$LIB_DIR/libdense_sim.so.0" "libdense_sim.so.0.1.0"
check_symlink "$LIB_DIR/libdensedb.so" "libdensedb.so.0"
check_symlink "$LIB_DIR/libdensedb.so.0" "libdensedb.so.0.1.0"

check_elf() {
    local path="$1"
    local soname="$2"
    [[ -f "$path" ]] || {
        error "missing ELF library: ${path#"$ROOT_DIR/"}"
        return
    }
    if command -v file >/dev/null 2>&1; then
        local output
        output="$(file -b "$path")"
        [[ "$output" == *"ELF 64-bit"* && "$output" == *"x86-64"* ]] \
            || error "unexpected ELF format: ${path#"$ROOT_DIR/"}: $output"
    fi
    if command -v readelf >/dev/null 2>&1; then
        readelf -d "$path" 2>/dev/null | grep -Fq "Library soname: [$soname]" \
            || error "missing or incorrect SONAME in ${path#"$ROOT_DIR/"}"
    fi
}

check_elf "$LIB_DIR/libdense_sim.so.0.1.0" "libdense_sim.so.0"
check_elf "$LIB_DIR/libdensedb.so.0.1.0" "libdensedb.so.0"

for archive in "$LIB_DIR/libdense_sim.a" "$LIB_DIR/libdensedb.a"; do
    [[ -f "$archive" ]] || error "missing static archive: ${archive#"$ROOT_DIR/"}"
    if [[ -f "$archive" ]] && command -v ar >/dev/null 2>&1; then
        ar t "$archive" >/dev/null || error "invalid static archive: ${archive#"$ROOT_DIR/"}"
    fi
done

if command -v readelf >/dev/null 2>&1 && [[ -f "$LIB_DIR/libdensedb.so.0.1.0" ]]; then
    readelf -d "$LIB_DIR/libdensedb.so.0.1.0" | grep -Fq "Shared library: [libdense_sim.so.0]" \
        || error "libdensedb does not declare libdense_sim.so.0 dependency"
fi

check_exports() {
    local library="$1"
    local expected_file="$2"
    local label="$3"
    command -v nm >/dev/null 2>&1 || {
        warn "nm unavailable; skipped exported-symbol audit for $label"
        return
    }
    local actual
    actual="$(mktemp)"
    nm -D --defined-only "$library" \
        | awk '$2 ~ /^[TW]$/ {print $3}' \
        | sed 's/@@.*//' \
        | sort -u > "$actual"
    if ! diff -u "$expected_file" "$actual" >/dev/null; then
        error "$label exported symbols differ from ${expected_file#"$ROOT_DIR/"}"
        diff -u "$expected_file" "$actual" >&2 || true
    fi
    rm -f -- "$actual"
}

if [[ -f "$LIB_DIR/libdense_sim.so.0.1.0" && -f "$ROOT_DIR/release/abi/libdense_sim.exports" ]]; then
    check_exports "$LIB_DIR/libdense_sim.so.0.1.0" "$ROOT_DIR/release/abi/libdense_sim.exports" "libdense_sim"
fi
if [[ -f "$LIB_DIR/libdensedb.so.0.1.0" && -f "$ROOT_DIR/release/abi/libdensedb.exports" ]]; then
    check_exports "$LIB_DIR/libdensedb.so.0.1.0" "$ROOT_DIR/release/abi/libdensedb.exports" "libdensedb"
fi

for template in pkgconfig/libdense_sim.pc.in pkgconfig/libdensedb.pc.in; do
    require_file "$template"
    if [[ -f "$ROOT_DIR/$template" ]]; then
        grep -Fq '@PREFIX@' "$ROOT_DIR/$template" || error "missing @PREFIX@ in $template"
        grep -Fq '@LIBDIR@' "$ROOT_DIR/$template" || error "missing @LIBDIR@ in $template"
        grep -Fq '@VERSION@' "$ROOT_DIR/$template" || error "missing @VERSION@ in $template"
    fi
done

wheel_count=0
while IFS= read -r -d "" wheel; do
    ((wheel_count += 1))
    python3 -m zipfile -t "$wheel" >/dev/null || error "invalid Python wheel: ${wheel#"$ROOT_DIR/"}"
    if unzip -p "$wheel" 'dense_sim/__init__.py' 2>/dev/null | grep -Fq '0.1.0.dev'; then
        error "development version remains in wheel: ${wheel#"$ROOT_DIR/"}"
    fi
done < <(find "$ROOT_DIR/bindings/python/dist" -maxdepth 1 -type f -name '*.whl' -print0)
((wheel_count == 2)) || error "expected two Python wheels, found $wheel_count"

if command -v python3 >/dev/null 2>&1; then
    python3 - "$ROOT_DIR/bindings/python/setup.py" "$ROOT_DIR/bindings/check_abi_coverage.py" <<'PY_CHECK' \
        || error "Python source syntax validation failed"
from pathlib import Path
import sys

for argument in sys.argv[1:]:
    path = Path(argument)
    compile(path.read_text(encoding="utf-8"), str(path), "exec")
PY_CHECK
    python3 "$ROOT_DIR/bindings/check_abi_coverage.py" \
        || error "binding ABI coverage check failed"
fi


if [[ -f "$ROOT_DIR/SHA256SUMS" ]] && command -v sha256sum >/dev/null 2>&1; then
    if ! (cd "$ROOT_DIR" && sha256sum --check --quiet SHA256SUMS); then
        error "SHA256SUMS validation failed"
    fi
fi

if grep -Eq '^\*\.so$|^\*\.a$' "$ROOT_DIR/.gitignore" 2>/dev/null; then
    error ".gitignore excludes release libraries"
fi

if ((ERRORS > 0)); then
    printf "Release verification failed: %d error(s), %d warning(s).\n" "$ERRORS" "$WARNINGS" >&2
    exit 1
fi

printf "Release verification passed with %d warning(s).\n" "$WARNINGS"
