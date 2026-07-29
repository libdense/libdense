#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ERRORS=0
WARNINGS=0

RELEASE_VERSION="0.2.0"

LIBRARIES=(
    dense_sim
    dense_net
    dense_sched
    dense_collision
    dense_nav
    dense_ai
    densedb
)

HEADERS=(
    dense_sim.h
    dense_net.h
    dense_net_sim_bridge.h
    dense_sched.h
    dense_collision.h
    dense_nav.h
    dense_nav_collision_bridge.h
    dense_ai.h
    densedb.h
)

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
    install.sh \
    uninstall.sh \
    verify-release.sh \
    tools/generate-checksums.sh \
    SHA256SUMS \
    bindings/cpp/include/dense/dense_sim.hpp; do
    require_file "$relative"
done

for header in "${HEADERS[@]}"; do
    require_file "include/dense/$header"
done

for library in "${LIBRARIES[@]}"; do
    require_file "release/abi/lib$library.exports"
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
    [[ "$VERSION" == "$RELEASE_VERSION" ]] \
        || error "VERSION is $VERSION; expected $RELEASE_VERSION"
fi

for forbidden in \
    .git \
    dense_core \
    libdense_sim \
    libdense_net \
    libdense_sched \
    libdense_collision \
    libdense_nav \
    libdense_ai \
    densedb \
    integration \
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

# Images are permitted only in the documented locations: the two logos and
# the densebench/ chart directory referenced by README benchmark sections.
while IFS= read -r -d "" image; do
    relative="${image#"$ROOT_DIR/"}"
    case "$relative" in
        logo.png|logo-square.png|densebench/*)
            ;;
        *)
            error "image artifact outside documented locations: $relative"
            ;;
    esac
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

for library in "${LIBRARIES[@]}"; do
    real="$LIB_DIR/lib$library.so.$RELEASE_VERSION"
    check_symlink "$LIB_DIR/lib$library.so" "lib$library.so.0"
    check_symlink "$LIB_DIR/lib$library.so.0" "lib$library.so.$RELEASE_VERSION"
    check_elf "$real" "lib$library.so.0"

    archive="$LIB_DIR/lib$library.a"
    [[ -f "$archive" ]] || error "missing static archive: ${archive#"$ROOT_DIR/"}"
    if [[ -f "$archive" ]] && command -v ar >/dev/null 2>&1; then
        ar t "$archive" >/dev/null || error "invalid static archive: ${archive#"$ROOT_DIR/"}"
    fi

    if [[ -f "$real" && -f "$ROOT_DIR/release/abi/lib$library.exports" ]]; then
        check_exports "$real" "$ROOT_DIR/release/abi/lib$library.exports" "lib$library"
    fi
done

if command -v readelf >/dev/null 2>&1 && [[ -f "$LIB_DIR/libdensedb.so.$RELEASE_VERSION" ]]; then
    readelf -d "$LIB_DIR/libdensedb.so.$RELEASE_VERSION" | grep -Fq "Shared library: [libdense_sim.so.0]" \
        || error "libdensedb does not declare libdense_sim.so.0 dependency"
fi

# No shipped library may export private dense_core symbols.
if command -v nm >/dev/null 2>&1; then
    for library in "${LIBRARIES[@]}"; do
        real="$LIB_DIR/lib$library.so.$RELEASE_VERSION"
        [[ -f "$real" ]] || continue
        if nm -D --defined-only "$real" | awk '{print $3}' | grep -Eq '^dc_(arena|dirty_set|slot_pool|group_map|span_hash|cpu_features)_'; then
            error "private dense_core symbol exported by lib$library"
        fi
    done
fi

for library in "${LIBRARIES[@]}"; do
    template="pkgconfig/lib$library.pc.in"
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
    if unzip -p "$wheel" 'dense_sim/__init__.py' 2>/dev/null | grep -Fq "$RELEASE_VERSION.dev"; then
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

if ((ERRORS > 0)); then
    printf "Release verification failed: %d error(s), %d warning(s).\n" "$ERRORS" "$WARNINGS" >&2
    exit 1
fi

printf "Release verification passed with %d warning(s).\n" "$WARNINGS"
