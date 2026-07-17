#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${DENSE_ROOT:-$SCRIPT_DIR}"
PYTHON_PACKAGE="$ROOT/bindings/python"
BUILDER="$ROOT/.wheel-builder"
DIST_DIR="$PYTHON_PACKAGE/dist"
RAW_WHEELS="$DIST_DIR/raw"
FINAL_WHEELS="$DIST_DIR/wheelhouse"
LOG_FILE="$ROOT/build-python-wheels.log"

finish() {
    local status="$?"

    echo
    if [[ "$status" -eq 0 ]]; then
        echo "Dense Python wheel build completed successfully."
        echo "Final wheels: $FINAL_WHEELS"
    else
        echo "Dense Python wheel build FAILED with exit code $status."
        echo "Build log: $LOG_FILE"
    fi

    if [[ -t 0 && "${DENSE_NO_PAUSE:-0}" != "1" ]]; then
        echo
        read -r -p "Press Enter to close..." _
    fi
}

show_error() {
    local status="$?"
    echo
    echo "ERROR on line $1 while running: $2"
    return "$status"
}

trap 'show_error "$LINENO" "$BASH_COMMAND"' ERR
trap finish EXIT

mkdir -p "$ROOT"
touch "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

printf "\nDense Python wheel builder\n"
printf "Repository: %s\n" "$ROOT"
printf "Started:    %s\n\n" "$(date --iso-8601=seconds)"

if [[ ! -f "$PYTHON_PACKAGE/pyproject.toml" ]]; then
    echo "ERROR: $PYTHON_PACKAGE/pyproject.toml was not found."
    echo "Place this script in the Dense repository root, or run:"
    echo "  DENSE_ROOT=/path/to/dense ./build_python_wheels.sh"
    exit 1
fi

if [[ ! -f "$PYTHON_PACKAGE/setup.py" ]]; then
    echo "ERROR: $PYTHON_PACKAGE/setup.py was not found."
    exit 1
fi

find_python() {
    local version="$1"
    local executable="python${version}"
    local candidate=""

    if command -v "$executable" >/dev/null 2>&1; then
        command -v "$executable"
        return 0
    fi

    for candidate in \
        "/usr/local/bin/$executable" \
        "/usr/bin/$executable" \
        "$HOME/.local/bin/$executable"
    do
        if [[ -x "$candidate" ]]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done

    echo "ERROR: could not find $executable" >&2
    return 1
}

declare -A PYTHONS=()

for version in "3.11" "3.12" "3.13" "3.14"; do
    PYTHONS["$version"]="$(find_python "$version")"
done

echo "Detected Python interpreters:"
for version in "3.11" "3.12" "3.13" "3.14"; do
    printf "  Python %s: %s\n" "$version" "${PYTHONS[$version]}"
    "${PYTHONS[$version]}" --version

done

install_native_dependencies() {
    local need_packages=0

    for command_name in make cc patchelf readelf sha256sum; do
        if ! command -v "$command_name" >/dev/null 2>&1; then
            need_packages=1
        fi
    done

    if [[ "$need_packages" -eq 0 ]]; then
        return 0
    fi

    if ! command -v apt-get >/dev/null 2>&1; then
        echo "ERROR: required native tools are missing and apt-get is unavailable."
        echo "Install a C compiler, make, binutils, and patchelf, then rerun."
        exit 1
    fi

    echo
    echo "Installing native build dependencies..."

    if command -v sudo >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y build-essential binutils patchelf
    else
        apt-get update
        apt-get install -y build-essential binutils patchelf
    fi
}

install_native_dependencies

export PATH="$HOME/.local/bin:$PATH"

if ! command -v poetry >/dev/null 2>&1; then
    POETRY_VENV="$HOME/.local/share/dense-poetry"
    POETRY_PYTHON="${PYTHONS[3.11]}"

    echo
    echo "Poetry was not found. Installing it into:"
    echo "  $POETRY_VENV"

    rm -rf "$POETRY_VENV"
    "$POETRY_PYTHON" -m venv "$POETRY_VENV"

    "$POETRY_VENV/bin/python" -m pip install \
        --upgrade \
        pip \
        setuptools \
        wheel

    "$POETRY_VENV/bin/python" -m pip install \
        "poetry>=2,<3"

    mkdir -p "$HOME/.local/bin"
    ln -sfn "$POETRY_VENV/bin/poetry" "$HOME/.local/bin/poetry"
    export PATH="$HOME/.local/bin:$PATH"
fi

echo
poetry --version

rm -rf "$BUILDER"
mkdir -p "$BUILDER"

cat > "$BUILDER/pyproject.toml" << "EOF"
[tool.poetry]
name = "dense-wheel-builder"
version = "0.1.0"
description = "Poetry environments for building Dense Python wheels"
authors = []
package-mode = false

[tool.poetry.dependencies]
python = ">=3.11,<3.15"

[tool.poetry.group.build]
optional = true

[tool.poetry.group.build.dependencies]
build = ">=1.2"
wheel = ">=0.43"
auditwheel = ">=6.0"
twine = ">=5.0"

[build-system]
requires = ["poetry-core>=2,<3"]
build-backend = "poetry.core.masonry.api"
EOF

cd "$BUILDER"

poetry config virtualenvs.create true --local
poetry config virtualenvs.in-project false --local

poetry lock --no-interaction

rm -rf \
    "$PYTHON_PACKAGE/build" \
    "$DIST_DIR" \
    "$PYTHON_PACKAGE/src/dense_sim.egg-info"

mkdir -p "$RAW_WHEELS" "$FINAL_WHEELS"

for version in "3.11" "3.12" "3.13" "3.14"; do
    PYTHON_BIN="${PYTHONS[$version]}"

    echo
    echo "============================================================"
    echo "Building Dense wheel with Python $version"
    echo "Interpreter: $PYTHON_BIN"
    echo "============================================================"

    poetry env use "$PYTHON_BIN"
    poetry install --with build --no-interaction

    poetry run python --version

    rm -rf \
        "$PYTHON_PACKAGE/build" \
        "$PYTHON_PACKAGE/src/dense_sim.egg-info"

    poetry run python -m build \
        --wheel \
        --outdir "$RAW_WHEELS" \
        "$PYTHON_PACKAGE"
done

echo
echo "Raw wheels:"
find "$RAW_WHEELS" \
    -maxdepth 1 \
    -type f \
    -name "*.whl" \
    -printf "  %f\n" \
    | sort

RAW_COUNT="$(
    find "$RAW_WHEELS" \
        -maxdepth 1 \
        -type f \
        -name "*.whl" \
        | wc -l
)"

if [[ "$RAW_COUNT" -ne 4 ]]; then
    echo "ERROR: expected 4 raw wheels, found $RAW_COUNT."
    exit 1
fi

echo
echo "Repairing wheels as manylinux_2_17_x86_64..."

poetry env use "${PYTHONS[3.13]}"
poetry install --with build --no-interaction

for wheel in "$RAW_WHEELS"/*.whl; do
    echo
    echo "Inspecting $(basename "$wheel")"
    poetry run auditwheel show "$wheel"

    poetry run auditwheel repair \
        --plat "manylinux_2_17_x86_64" \
        --wheel-dir "$FINAL_WHEELS" \
        "$wheel"
done

echo
echo "Checking final wheel metadata..."
poetry run python -m twine check "$FINAL_WHEELS"/*.whl

FINAL_COUNT="$(
    find "$FINAL_WHEELS" \
        -maxdepth 1 \
        -type f \
        -name "*.whl" \
        | wc -l
)"

if [[ "$FINAL_COUNT" -ne 4 ]]; then
    echo "ERROR: expected 4 final wheels, found $FINAL_COUNT."
    exit 1
fi

echo
echo "Testing each wheel with its matching interpreter..."

for version in "3.11" "3.12" "3.13" "3.14"; do
    PYTHON_BIN="${PYTHONS[$version]}"
    CP_TAG="${version/./}"

    WHEEL="$(
        find "$FINAL_WHEELS" \
            -maxdepth 1 \
            -type f \
            -name "*-cp${CP_TAG}-cp${CP_TAG}-*.whl" \
            -print \
            -quit
    )"

    if [[ -z "$WHEEL" ]]; then
        echo "ERROR: no cp${CP_TAG} wheel was found."
        exit 1
    fi

    echo
    echo "Testing Python $version with $(basename "$WHEEL")"

    poetry env use "$PYTHON_BIN"
    poetry install --with build --no-interaction

    poetry run python -m pip uninstall -y dense-sim >/dev/null 2>&1 || true
    poetry run python -m pip install \
        --force-reinstall \
        --no-deps \
        "$WHEEL"

    poetry run python -c "import dense_sim, sys; print(f\"Python {sys.version.split()[0]}: dense_sim {dense_sim.__version__}\")"
done

(
    cd "$FINAL_WHEELS"
    sha256sum ./*.whl > "$DIST_DIR/SHA256SUMS"
)

echo
echo "============================================================"
echo "BUILD COMPLETE"
echo "============================================================"

echo
echo "Poetry environments:"
poetry env list --full-path

echo
echo "Final PyPI wheels:"
find "$FINAL_WHEELS" \
    -maxdepth 1 \
    -type f \
    -name "*.whl" \
    -printf "  %p\n" \
    | sort

echo
echo "SHA-256 checksums:"
cat "$DIST_DIR/SHA256SUMS"

echo
echo "Upload only the wheels from:"
echo "  $FINAL_WHEELS"
