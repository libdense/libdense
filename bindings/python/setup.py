from __future__ import annotations

import os
from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

# This release package ships a prebuilt static library and public headers.
# DENSE_SIM_LIB_DIR / DENSE_SIM_INCLUDE_DIR override the defaults, and the
# development monorepo layout is used as a fallback when present.
_DEV_LIBDENSE = ROOT / "libdense_sim"


def _default_lib_dir() -> Path:
    override = os.environ.get("DENSE_SIM_LIB_DIR")
    if override:
        return Path(override)
    release = ROOT / "lib" / "linux-x86_64"
    if (release / "libdense_sim.a").is_file():
        return release
    return _DEV_LIBDENSE / "build" / "lib"


def _default_include_dir() -> Path:
    override = os.environ.get("DENSE_SIM_INCLUDE_DIR")
    if override:
        return Path(override)
    release = ROOT / "include" / "dense"
    if (release / "dense_sim.h").is_file():
        return release
    return _DEV_LIBDENSE / "include"


LIB_DIR = _default_lib_dir()
INCLUDE_DIR = _default_include_dir()
STATIC_LIBRARY = Path(
    os.environ.get("DENSE_SIM_STATIC_LIBRARY", LIB_DIR / "libdense_sim.a")
)


class DenseBuildExt(build_ext):
    def run(self) -> None:
        if not STATIC_LIBRARY.is_file():
            raise RuntimeError(
                f"libdense_sim.a not found at {STATIC_LIBRARY}; "
                "set DENSE_SIM_LIB_DIR"
            )
        super().run()


setup(
    name="dense-sim",
    version="0.2.0",
    description="Python binding for libdense_sim",
    package_dir={"": "src"},
    packages=["dense_sim"],
    package_data={"dense_sim": ["py.typed", "__init__.pyi"]},
    ext_modules=[
        Extension(
            "dense_sim._dense_sim",
            sources=["src/dense_sim/_dense_sim.c"],
            include_dirs=[str(INCLUDE_DIR)],
            extra_objects=[str(STATIC_LIBRARY)],
            libraries=["m"],
            extra_compile_args=[
                "-std=c11",
                "-O3",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Wconversion",
                "-Wshadow",
            ],
        )
    ],
    cmdclass={"build_ext": DenseBuildExt},
    python_requires=">=3.11",
)
