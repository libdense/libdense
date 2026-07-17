#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-}"
LIBDIR="${LIBDIR:-lib}"
INSTALL_DOCS=1
INSTALL_CPP=1
RUN_LDCONFIG=1
DRY_RUN=0

usage() {
    cat <<"USAGE"
Usage: ./install.sh [options]

Install the precompiled Dense native SDK.

Options:
  --prefix PATH       Installation prefix (default: /usr/local)
  --destdir PATH      Staging root prepended to every installed path
  --libdir NAME       Library directory below PREFIX (default: lib)
  --no-docs           Do not install documentation
  --no-cpp            Do not install the C++ wrapper header
  --no-ldconfig       Do not refresh the dynamic-linker cache
  --dry-run           Print operations without modifying the filesystem
  -h, --help          Show this help
USAGE
}

fail() {
    printf "install.sh: %s\n" "$*" >&2
    exit 1
}

while (($# > 0)); do
    case "$1" in
        --prefix)
            (($# >= 2)) || fail "--prefix requires a value"
            PREFIX="$2"
            shift 2
            ;;
        --destdir)
            (($# >= 2)) || fail "--destdir requires a value"
            DESTDIR="$2"
            shift 2
            ;;
        --libdir)
            (($# >= 2)) || fail "--libdir requires a value"
            LIBDIR="$2"
            shift 2
            ;;
        --no-docs)
            INSTALL_DOCS=0
            shift
            ;;
        --no-cpp)
            INSTALL_CPP=0
            shift
            ;;
        --no-ldconfig)
            RUN_LDCONFIG=0
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

[[ "$PREFIX" == /* ]] || fail "PREFIX must be an absolute path"
if [[ -n "$DESTDIR" ]]; then
    [[ "$DESTDIR" == /* ]] || fail "DESTDIR must be an absolute path"
    DESTDIR="${DESTDIR%/}"
fi
[[ "$LIBDIR" != /* ]] || fail "LIBDIR must be relative to PREFIX"
[[ "$LIBDIR" != *".."* ]] || fail "LIBDIR must not contain '..'"
[[ "$LIBDIR" != "" ]] || fail "LIBDIR must not be empty"

PREFIX="${PREFIX%/}"
[[ -n "$PREFIX" ]] || PREFIX="/"

host_os="$(uname -s)"
host_arch="$(uname -m)"
if [[ -z "$DESTDIR" && "$host_os" != "Linux" ]]; then
    fail "this release targets Linux; detected $host_os"
fi
if [[ -z "$DESTDIR" && "$host_arch" != "x86_64" && "$host_arch" != "amd64" ]]; then
    fail "this release targets Linux x86-64; detected $host_arch"
fi

VERSION_FILE="$ROOT_DIR/VERSION"
[[ -f "$VERSION_FILE" ]] || fail "missing VERSION"
VERSION="$(tr -d "[:space:]" < "$VERSION_FILE")"
[[ -n "$VERSION" ]] || fail "VERSION is empty"

HEADER_DIR="$ROOT_DIR/include/dense"
[[ -f "$HEADER_DIR/dense_sim.h" ]] || fail "missing include/dense/dense_sim.h"
[[ -f "$HEADER_DIR/densedb.h" ]] || fail "missing include/dense/densedb.h"

LIB_SOURCE=""
for candidate in \
    "$ROOT_DIR/lib/linux-x86_64" \
    "$ROOT_DIR/lib/x86_64" \
    "$ROOT_DIR/lib"; do
    if [[ -d "$candidate" ]]; then
        LIB_SOURCE="$candidate"
        if find "$candidate" -maxdepth 1 \( -type f -o -type l \) \
            \( -name "libdense_sim.so*" -o -name "libdense_sim.a" \
               -o -name "libdensedb.so*" -o -name "libdensedb.a" \) \
            -print -quit | grep -q .; then
            break
        fi
    fi
done

[[ -n "$LIB_SOURCE" ]] || fail "missing native library directory"

dense_count=0
densedb_count=0
while IFS= read -r -d "" artifact; do
    base="$(basename -- "$artifact")"
    case "$base" in
        libdense_sim.so*|libdense_sim.a)
            ((dense_count += 1))
            ;;
        libdensedb.so*|libdensedb.a)
            ((densedb_count += 1))
            ;;
    esac
done < <(
    find "$LIB_SOURCE" -maxdepth 1 \( -type f -o -type l \) \
        \( -name "libdense_sim.so*" -o -name "libdense_sim.a" \
           -o -name "libdensedb.so*" -o -name "libdensedb.a" \) \
        -print0
)

((dense_count > 0)) || fail "no libdense_sim artifact found in $LIB_SOURCE"
((densedb_count > 0)) || fail "no libdensedb artifact found in $LIB_SOURCE"

[[ -f "$ROOT_DIR/pkgconfig/libdense_sim.pc.in" ]] || fail "missing libdense_sim.pc.in"
[[ -f "$ROOT_DIR/pkgconfig/libdensedb.pc.in" ]] || fail "missing libdensedb.pc.in"

TMP_DIR="$(mktemp -d)"
MANIFEST_TMP="$TMP_DIR/install-manifest.txt"
trap 'rm -rf -- "$TMP_DIR"' EXIT

logical_path() {
    local relative="$1"
    if [[ "$PREFIX" == "/" ]]; then
        printf "/%s" "$relative"
    else
        printf "%s/%s" "$PREFIX" "$relative"
    fi
}

physical_path() {
    local logical="$1"
    printf "%s%s" "$DESTDIR" "$logical"
}

show_command() {
    printf "+"
    printf " %q" "$@"
    printf "\n"
}

run_command() {
    if ((DRY_RUN)); then
        show_command "$@"
    else
        "$@"
    fi
}

ensure_directory() {
    local directory="$1"
    run_command install -d -m 0755 -- "$directory"
}

record_path() {
    printf "%s\n" "$1" >> "$MANIFEST_TMP"
}

install_regular() {
    local source="$1"
    local relative="$2"
    local mode="$3"
    local logical destination

    logical="$(logical_path "$relative")"
    destination="$(physical_path "$logical")"
    ensure_directory "$(dirname -- "$destination")"
    run_command install -m "$mode" -- "$source" "$destination"
    record_path "$logical"
}

install_library_artifact() {
    local source="$1"
    local relative="$2"
    local logical destination target mode

    logical="$(logical_path "$relative")"
    destination="$(physical_path "$logical")"
    ensure_directory "$(dirname -- "$destination")"

    if [[ -L "$source" ]]; then
        target="$(readlink -- "$source")"
        [[ "$target" != /* ]] || fail "absolute library symlink is not allowed: $source -> $target"
        run_command rm -f -- "$destination"
        run_command ln -s -- "$target" "$destination"
    else
        case "$source" in
            *.a)
                mode="0644"
                ;;
            *)
                mode="0755"
                ;;
        esac
        run_command install -m "$mode" -- "$source" "$destination"
    fi

    record_path "$logical"
}

render_pkgconfig() {
    local source="$1"
    local output="$2"

    sed \
        -e "s|@PREFIX@|$PREFIX|g" \
        -e "s|@LIBDIR@|$LIBDIR|g" \
        -e "s|@VERSION@|$VERSION|g" \
        "$source" > "$output"
}

printf "Installing Dense %s\n" "$VERSION"
printf "  prefix:  %s\n" "$PREFIX"
printf "  destdir: %s\n" "${DESTDIR:-<none>}"
printf "  libdir:  %s\n" "$LIBDIR"

install_regular "$HEADER_DIR/dense_sim.h" "include/dense/dense_sim.h" 0644
install_regular "$HEADER_DIR/densedb.h" "include/dense/densedb.h" 0644

CPP_HEADER="$ROOT_DIR/bindings/cpp/include/dense/dense_sim.hpp"
if ((INSTALL_CPP)) && [[ -f "$CPP_HEADER" ]]; then
    install_regular "$CPP_HEADER" "include/dense/dense_sim.hpp" 0644
fi

while IFS= read -r -d "" artifact; do
    install_library_artifact "$artifact" "$LIBDIR/$(basename -- "$artifact")"
done < <(
    find "$LIB_SOURCE" -maxdepth 1 \( -type f -o -type l \) \
        \( -name "libdense_sim.so*" -o -name "libdense_sim.a" \
           -o -name "libdensedb.so*" -o -name "libdensedb.a" \) \
        -print0 | sort -z
)

render_pkgconfig "$ROOT_DIR/pkgconfig/libdense_sim.pc.in" "$TMP_DIR/libdense_sim.pc"
render_pkgconfig "$ROOT_DIR/pkgconfig/libdensedb.pc.in" "$TMP_DIR/libdensedb.pc"
install_regular "$TMP_DIR/libdense_sim.pc" "$LIBDIR/pkgconfig/libdense_sim.pc" 0644
install_regular "$TMP_DIR/libdensedb.pc" "$LIBDIR/pkgconfig/libdensedb.pc" 0644

if ((INSTALL_DOCS)); then
    for document in \
        README.md \
        CHANGELOG.md \
        LICENSE.md \
        COMMERCIAL-LICENSE.md \
        SECURITY.md \
        VERSION \
        MANIFEST.md \
        CONTRIBUTING.md \
        SUPPORT.md; do
        if [[ -f "$ROOT_DIR/$document" ]]; then
            install_regular "$ROOT_DIR/$document" "share/doc/dense/$document" 0644
        fi
    done

    while IFS= read -r -d "" document; do
        install_regular "$document" "share/doc/dense/docs/$(basename -- "$document")" 0644
    done < <(find "$ROOT_DIR/docs" -maxdepth 1 -type f -name "*.md" -print0 | sort -z)
fi

manifest_logical="$(logical_path "share/dense/install-manifest.txt")"
printf "%s\n" "$manifest_logical" >> "$MANIFEST_TMP"
manifest_destination="$(physical_path "$manifest_logical")"
ensure_directory "$(dirname -- "$manifest_destination")"
run_command install -m 0644 -- "$MANIFEST_TMP" "$manifest_destination"

if ((RUN_LDCONFIG)) && [[ -z "$DESTDIR" ]] && command -v ldconfig >/dev/null 2>&1; then
    if ((EUID == 0)); then
        run_command ldconfig
    else
        printf "Not running ldconfig without root privileges.\n"
        printf "Run sudo ldconfig if the selected prefix requires it.\n"
    fi
fi

if ((DRY_RUN)); then
    printf "Dry run complete; no files were changed.\n"
else
    printf "Dense %s installed successfully.\n" "$VERSION"
    printf "Installation manifest: %s\n" "$manifest_destination"
fi
