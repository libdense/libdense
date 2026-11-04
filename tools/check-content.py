#!/usr/bin/env python3
"""Check the exact public payload, relative links and wheel contents."""
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    for name in ("dense_net_grouped", "dense_delivery", "dense_delivery_sim", "dense_sched_time"):
        header = (ROOT / "include/dense" / f"{name}.h").read_text()
        header = re.sub(r"/\*.*?\*/", "", header, flags=re.S)
        header = re.sub(r"//[^\n]*", "", header)
        canonical = "\n".join(" ".join(line.strip().split()) for line in header.splitlines() if line.strip()) + "\n"
        if canonical != (ROOT / "release/api" / f"{name}.api").read_text():
            raise ValueError(f"{name} SDK header differs from the frozen public API")
    manifest = json.loads((ROOT / "release/files.json").read_text())
    paths = {p.relative_to(ROOT).as_posix(): p for p in ROOT.rglob("*") if p.is_file() or p.is_symlink()}
    expected = set(manifest["files"]) | {"release/files.json", "SHA256SUMS"}
    if set(paths) != expected:
        raise ValueError(f"Payload inventory differs: {sorted(set(paths) ^ expected)}")
    for name, path in paths.items():
        if any(part in {"research", "private", "dense_core", "__pycache__", ".git"} for part in path.relative_to(ROOT).parts):
            raise ValueError(f"Private/generated path: {name}")
        if path.suffix.lower() in {".patch", ".log", ".o", ".pyc", ".pyo", ".bin", ".tar", ".gz", ".zip"}:
            raise ValueError(f"Unapproved artifact: {name}")
        if path.suffix.lower() in {".c", ".cc", ".cpp", ".cxx", ".rs"} and not name.startswith("bindings/"):
            raise ValueError(f"Implementation source outside public bindings: {name}")
        if path.is_symlink():
            target = path.readlink()
            if target.is_absolute() or not path.exists() or not path.resolve().is_relative_to(ROOT):
                raise ValueError(f"Invalid public symlink: {name}")
            if manifest["files"].get(name, {}).get("target") != str(target):
                raise ValueError(f"Symlink identity differs: {name}")
        elif name in manifest["files"]:
            if hashlib.sha256(path.read_bytes()).hexdigest() != manifest["files"][name]["sha256"]:
                raise ValueError(f"Payload identity differs: {name}")
        if path.suffix == ".sh" and path.stat().st_mode & 0o111 != 0o111:
            raise ValueError(f"Shell entry point is not executable: {name}")
        if path.suffix == ".whl":
            with zipfile.ZipFile(path) as archive:
                names = archive.namelist()
                if len(names) != len(set(names)) or archive.testzip() is not None:
                    raise ValueError(f"Invalid wheel: {name}")
                for member in names:
                    q = Path(member)
                    if q.is_absolute() or ".." in q.parts or q.suffix in {".c", ".h", ".a", ".o", ".zip"}:
                        raise ValueError(f"Unapproved wheel member: {name}: {member}")
        if path.suffix == ".md":
            text = re.sub(r"```.*?```", "", path.read_text(), flags=re.S)
            for raw in re.findall(r"(?<!!)\[[^\]]*\]\(([^)]+)\)", text):
                link = raw.split("#")[0].split()[0] if raw else ""
                if not link or "://" in link or link.startswith(("mailto:", "#")):
                    continue
                if not (path.parent / link).resolve().exists():
                    raise ValueError(f"Broken documentation link: {name}: {link}")
    print(f"Public payload verified: {len(paths)} files/links; no core/demo implementation, research, logs or source archives")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, KeyError, ValueError, OSError, zipfile.BadZipFile) as error:
        print(f"Public payload check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
