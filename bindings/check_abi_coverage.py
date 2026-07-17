#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "dense" / "dense_sim.h"
WRAPPERS = {
    "cpp": ROOT / "bindings" / "cpp" / "include" / "dense" / "dense_sim.hpp",
    "rust": ROOT / "bindings" / "rust" / "src" / "lib.rs",
}

EXPECTED_PUBLIC_FUNCTIONS = 28

C_TO_RUST_TYPES = {
    "bool": "bool",
    "const char *": "*const c_char",
    "const ds_chunk_delta *": "*const DsChunkDelta",
    "const ds_delta_entry *": "*const DsDeltaEntry",
    "const ds_motion_plan *": "*const DsMotionPlan",
    "const ds_observer_config *": "*const DsObserverConfig",
    "const ds_observer_id *": "*const u64",
    "const ds_world *": "*const DsWorld",
    "const ds_world_config *": "*const DsWorldConfig",
    "double": "f64",
    "ds_channel_mask": "u64",
    "ds_delta_op": "c_int",
    "ds_entity_desc *": "*mut DsEntityDesc",
    "ds_entity_id": "u64",
    "ds_fanout_view *": "*mut DsFanoutView",
    "ds_motion_metrics *": "*mut DsMotionMetrics",
    "ds_motion_mode *": "*mut c_int",
    "ds_observer_desc *": "*mut DsObserverDesc",
    "ds_observer_id": "u64",
    "ds_observer_id *": "*mut u64",
    "ds_result": "c_int",
    "ds_tick": "u64",
    "ds_type_mask": "u64",
    "ds_world *": "*mut DsWorld",
    "ds_world **": "*mut *mut DsWorld",
    "int32_t": "i32",
    "size_t": "usize",
    "uint64_t": "u64",
    "void": "()",
}

STRUCT_NAMES = {
    "ds_world_config": "DsWorldConfig",
    "ds_observer_config": "DsObserverConfig",
    "ds_observer_desc": "DsObserverDesc",
    "ds_entity_desc": "DsEntityDesc",
    "ds_motion_plan": "DsMotionPlan",
    "ds_motion_metrics": "DsMotionMetrics",
    "ds_delta_entry": "DsDeltaEntry",
    "ds_chunk_delta": "DsChunkDelta",
    "ds_fanout_view": "DsFanoutView",
}


def strip_c_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def normalize_c_type(c_type: str) -> str:
    value = " ".join(c_type.split())
    value = re.sub(r"\s*\*\s*", "*", value)
    value = re.sub(r"(\*+)", r" \1", value)
    return value.strip()


def map_c_type(c_type: str) -> str:
    normalized = normalize_c_type(c_type)
    try:
        return C_TO_RUST_TYPES[normalized]
    except KeyError as exc:
        raise ValueError(f"no Rust FFI type mapping for C type: {normalized}") from exc


def split_comma_list(text: str) -> list[str]:
    return [item.strip() for item in text.split(",") if item.strip()]


def parse_c_parameter_type(parameter: str) -> str:
    parameter = " ".join(parameter.split())
    match = re.fullmatch(r"(.+?)([A-Za-z_][A-Za-z0-9_]*)", parameter)
    if match is None:
        raise ValueError(f"unable to parse C parameter: {parameter}")
    return normalize_c_type(match.group(1))


def public_function_signatures(
    header_text: str,
) -> dict[str, tuple[list[str], str]]:
    clean_header = strip_c_comments(header_text)
    declaration_pattern = re.compile(
        r"^DS_API[ \t]+(.*?);",
        re.MULTILINE | re.DOTALL,
    )
    signatures: dict[str, tuple[list[str], str]] = {}

    for declaration in declaration_pattern.findall(clean_header):
        match = re.fullmatch(
            r"(.+?)(ds_[a-z0-9_]+)\s*\((.*?)\)\s*",
            declaration,
            re.DOTALL,
        )
        if match is None:
            raise ValueError(
                f"unable to parse public C ABI declaration: {declaration.strip()}"
            )

        return_type, name, parameters = match.groups()
        parameter_types = []
        if parameters.strip() and parameters.strip() != "void":
            parameter_types = [
                map_c_type(parse_c_parameter_type(parameter))
                for parameter in split_comma_list(parameters)
            ]
        signatures[name] = (parameter_types, map_c_type(return_type))

    return signatures


def rust_extern_signatures(
    rust_text: str,
) -> tuple[list[str], dict[str, tuple[list[str], str]]]:
    extern_match = re.search(r'extern\s+"C"\s*\{(.*?)\n\s*\}', rust_text, re.DOTALL)
    if extern_match is None:
        raise ValueError("unable to find Rust extern C block")

    extern_text = extern_match.group(1)
    pattern = re.compile(
        r"pub\s+fn\s+(ds_[a-z0-9_]+)\s*\((.*?)\)\s*(?:->\s*([^;]+))?;",
        re.DOTALL,
    )
    declarations: list[str] = []
    signatures: dict[str, tuple[list[str], str]] = {}

    for name, parameters, return_type in pattern.findall(extern_text):
        declarations.append(name)
        parameter_types = []
        for parameter in split_comma_list(parameters):
            if ":" not in parameter:
                raise ValueError(f"unable to parse Rust FFI parameter: {parameter}")
            _, rust_type = parameter.split(":", 1)
            parameter_types.append(" ".join(rust_type.split()))

        signatures[name] = (
            parameter_types,
            " ".join(return_type.split()) if return_type else "()",
        )

    return declarations, signatures


def c_struct_fields(header_text: str) -> dict[str, list[tuple[str, str]]]:
    clean_header = strip_c_comments(header_text)
    pattern = re.compile(
        r"typedef\s+struct\s+(ds_[a-z0-9_]+)\s*\{(.*?)\}\s*\1\s*;",
        re.DOTALL,
    )
    structures: dict[str, list[tuple[str, str]]] = {}

    for c_name, body in pattern.findall(clean_header):
        if c_name not in STRUCT_NAMES:
            continue
        fields: list[tuple[str, str]] = []
        for declaration in body.split(";"):
            declaration = " ".join(declaration.split())
            if not declaration:
                continue
            match = re.fullmatch(
                r"(.+?)([A-Za-z_][A-Za-z0-9_]*)",
                declaration,
            )
            if match is None:
                raise ValueError(
                    f"unable to parse C struct field in {c_name}: {declaration}"
                )
            fields.append((match.group(2), map_c_type(match.group(1))))
        structures[c_name] = fields

    return structures


def rust_struct_fields(rust_text: str, rust_name: str) -> list[tuple[str, str]]:
    pattern = re.compile(
        rf"pub\s+struct\s+{re.escape(rust_name)}\s*\{{(.*?)\n\s*\}}",
        re.DOTALL,
    )
    match = pattern.search(rust_text)
    if match is None:
        raise ValueError(f"unable to find Rust FFI struct {rust_name}")

    fields = []
    for field_name, rust_type in re.findall(
        r"pub\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([^,]+),",
        match.group(1),
    ):
        fields.append((field_name, " ".join(rust_type.split())))
    return fields


def main() -> int:
    header_text = HEADER.read_text(encoding="utf-8")
    rust_text = WRAPPERS["rust"].read_text(encoding="utf-8")

    try:
        c_signatures = public_function_signatures(header_text)
        rust_declarations, rust_signatures = rust_extern_signatures(rust_text)
        c_structures = c_struct_fields(header_text)
    except ValueError as exc:
        print(f"ABI coverage parser error: {exc}", file=sys.stderr)
        return 1

    functions = sorted(c_signatures)
    if len(functions) != EXPECTED_PUBLIC_FUNCTIONS:
        print(
            f"expected {EXPECTED_PUBLIC_FUNCTIONS} public C ABI functions, "
            f"found {len(functions)}",
            file=sys.stderr,
        )
        return 1

    failed = False
    rust_declaration_set = set(rust_declarations)

    if len(rust_declarations) != len(rust_declaration_set):
        failed = True
        duplicates = sorted(
            name
            for name in rust_declaration_set
            if rust_declarations.count(name) > 1
        )
        print(
            "rust FFI contains duplicate extern declarations:",
            file=sys.stderr,
        )
        for name in duplicates:
            print(f"  {name}", file=sys.stderr)

    missing_declarations = sorted(set(functions) - rust_declaration_set)
    extra_declarations = sorted(rust_declaration_set - set(functions))

    if missing_declarations or extra_declarations:
        failed = True
        if missing_declarations:
            print(
                "rust FFI is missing public C ABI declarations:",
                file=sys.stderr,
            )
            for name in missing_declarations:
                print(f"  {name}", file=sys.stderr)
        if extra_declarations:
            print(
                "rust FFI declares non-public or unknown ds_* functions:",
                file=sys.stderr,
            )
            for name in extra_declarations:
                print(f"  {name}", file=sys.stderr)

    for name in sorted(set(functions) & rust_declaration_set):
        if c_signatures[name] != rust_signatures[name]:
            failed = True
            print(
                f"rust FFI signature mismatch for {name}:\n"
                f"  C ABI mapped: {c_signatures[name]}\n"
                f"  Rust extern:  {rust_signatures[name]}",
                file=sys.stderr,
            )

    if not missing_declarations and not extra_declarations:
        print(
            f"rust FFI: signatures match all {len(functions)} public ABI functions"
        )

    try:
        for c_name, expected_fields in c_structures.items():
            rust_name = STRUCT_NAMES[c_name]
            actual_fields = rust_struct_fields(rust_text, rust_name)
            if expected_fields != actual_fields:
                failed = True
                print(
                    f"rust FFI struct field mismatch for {c_name}/{rust_name}:\n"
                    f"  C ABI mapped: {expected_fields}\n"
                    f"  Rust struct:  {actual_fields}",
                    file=sys.stderr,
                )
    except ValueError as exc:
        failed = True
        print(f"Rust FFI struct audit error: {exc}", file=sys.stderr)

    if not failed:
        print(
            f"rust FFI: field order/types match {len(c_structures)} public ABI structs"
        )

    for wrapper_name, wrapper_path in WRAPPERS.items():
        wrapper_text = wrapper_path.read_text(encoding="utf-8")
        missing = [name for name in functions if name not in wrapper_text]

        if missing:
            failed = True
            print(
                f"{wrapper_name} wrapper is missing ABI references:",
                file=sys.stderr,
            )
            for name in missing:
                print(f"  {name}", file=sys.stderr)
        else:
            print(
                f"{wrapper_name}: references all {len(functions)} public ABI functions"
            )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
