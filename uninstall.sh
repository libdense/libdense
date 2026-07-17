#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${DESTDIR:-}"
LIBDIR="${LIBDIR:-lib}"
RUN_LDCONFIG=1
DRY_RUN=0

usage() {
    cat <<"USAGE"
Usage: ./uninstall.sh [options]

Remove files recorded by the Dense installation manifest.

Options:
  --prefix PATH       Installation prefix used during install
  --destdir PATH      Staging root used during install
  --libdir NAME       Library directory used during install
  --no-ldconfig       Do not refresh the dynamic-linker cache
  --dry-run           Print operations without modifying the filesystem
  -h, --help          Show this help
USAGE
}

fail() {
    printf "uninstall.sh: %s\n" "$*" >&2
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

PREFIX="${PREFIX%/}"
[[ -n "$PREFIX" ]] || PREFIX="/"

logical_path() {
    local relative="$1"
    if [[ "$PREFIX" == "/" ]]; then
        printf "/%s" "$relative"
    else
        printf "%s/%s" "$PREFIX" "$relative"
    fi
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

manifest_logical="$(logical_path "share/dense/install-manifest.txt")"
manifest_path="$DESTDIR$manifest_logical"
[[ -f "$manifest_path" ]] || fail "installation manifest not found: $manifest_path"

mapfile -t installed_paths < "$manifest_path"
((${#installed_paths[@]} > 0)) || fail "installation manifest is empty"

for ((index = ${#installed_paths[@]} - 1; index >= 0; index--)); do
    logical="${installed_paths[index]}"
    [[ "$logical" == /* ]] || fail "unsafe non-absolute manifest entry: $logical"
    [[ "$logical" != *"/../"* && "$logical" != */.. ]] || fail "unsafe manifest entry: $logical"
    [[ "$logical" != "/" ]] || fail "unsafe manifest entry: /"

    if [[ "$PREFIX" != "/" ]]; then
        [[ "$logical" == "$PREFIX/"* ]] || fail "manifest entry is outside PREFIX: $logical"
    fi

    physical="$DESTDIR$logical"
    if [[ -e "$physical" || -L "$physical" ]]; then
        run_command rm -f -- "$physical"
    else
        printf "Already absent: %s\n" "$physical"
    fi
done

for relative_dir in \
    "share/dense" \
    "share/doc/dense/docs" \
    "share/doc/dense" \
    "$LIBDIR/pkgconfig" \
    "$LIBDIR" \
    "include/dense"; do
    logical="$(logical_path "$relative_dir")"
    physical="$DESTDIR$logical"
    if [[ -d "$physical" ]]; then
        if ((DRY_RUN)); then
            show_command rmdir --ignore-fail-on-non-empty -- "$physical"
        else
            rmdir --ignore-fail-on-non-empty -- "$physical" 2>/dev/null || true
        fi
    fi
done

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
    printf "Dense was uninstalled from %s.\n" "$PREFIX"
fi
