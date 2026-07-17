from __future__ import annotations

import os
from pathlib import Path

from setuptools import Extension, setup


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
INCLUDE_DIR = Path(
    os.environ.get(
        "DENSE_SIM_INCLUDE_DIR",
        ROOT / "include" / "dense",
    )
)
LIB_DIR = Path(
    os.environ.get(
        "DENSE_SIM_LIB_DIR",
        ROOT / "lib" / "linux-x86_64",
    )
)
STATIC_LIBRARY = Path(
    os.environ.get(
        "DENSE_SIM_STATIC_LIBRARY",
        LIB_DIR / "libdense_sim.a",
    )
)

if not (INCLUDE_DIR / "dense_sim.h").is_file():
    raise RuntimeError(
        f"dense_sim.h was not found in {INCLUDE_DIR}. "
        "Set DENSE_SIM_INCLUDE_DIR to the public header directory."
    )

if not STATIC_LIBRARY.is_file():
    raise RuntimeError(
        f"libdense_sim.a was not found at {STATIC_LIBRARY}. "
        "Set DENSE_SIM_STATIC_LIBRARY or DENSE_SIM_LIB_DIR."
    )

setup(
    name="dense-sim",
    version="0.1.0rc1",
    description="High-density spatial simulation bindings for libdense_sim",
    long_description=(HERE / "README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    python_requires=">=3.11",
    license_files=[
        "LICENSE.md",
        "COMMERCIAL-LICENSE.md",
    ],
    project_urls={
        "Documentation": "https://yggengine.com/",
        "Commercial Licensing": "https://yggengine.com/commercial",
    },
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
)
