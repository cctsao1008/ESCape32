"""Bind Application audit tools to the SAME ARM GCC used by CMake.

Fail closed when CMakeCache is missing or --gcc disagrees with the
configured toolchain. A PATH lookup that silently chooses a different
Newlib version must not be called an AM13E Application validation.
"""
import pathlib
import re
import shutil
import subprocess


def _resolve_binary(value):
    expanded = pathlib.Path(value).expanduser()
    if expanded.is_absolute():
        path = expanded
    elif "/" in value:
        path = pathlib.Path.cwd() / expanded
    else:
        found = shutil.which(value)
        if not found:
            raise RuntimeError("toolchain executable not found: " + value)
        path = pathlib.Path(found)
    path = path.resolve()
    if not path.is_file():
        raise RuntimeError("toolchain executable not found: " + str(path))
    return path


def configured_compiler(build_dir):
    cache = pathlib.Path(build_dir) / "CMakeCache.txt"
    if not cache.is_file():
        raise RuntimeError("CMakeCache not found: {}. Run CMake first.".format(cache))
    data = cache.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"^CMAKE_C_COMPILER:(?:FILEPATH|STRING)=(.+)$", data, re.MULTILINE
    )
    if match is None:
        raise RuntimeError("CMakeCache has no CMAKE_C_COMPILER: " + str(cache))
    return _resolve_binary(match.group(1).strip())


def matched_tools(build_dir, gcc=None, nm=None, objcopy=None):
    expected = configured_compiler(build_dir)
    actual = _resolve_binary(gcc) if gcc else expected
    if actual != expected:
        raise RuntimeError(
            "Compiler mismatch! Application CMake uses {}; requested {}. "
            "Choose the CMake compiler; do not mix ARM GCC/Newlib versions."
            .format(expected, actual)
        )
    if not actual.name.endswith("-gcc"):
        raise RuntimeError("Unexpected compiler basename: " + actual.name)
    base = actual.name[:-len("gcc")]
    def sibling(name, explicit):
        result = _resolve_binary(explicit) if explicit else _resolve_binary(
            str(actual.with_name(base + name))
        )
        if result.parent != actual.parent:
            raise RuntimeError(
                "{} belongs to different toolchain: {} vs {}"
                .format(name, result, actual)
            )
        return result
    nm_path = sibling("nm", nm)
    objcopy_path = sibling("objcopy", objcopy) if objcopy is not False else None
    version = subprocess.run(
        [str(actual), "-dumpfullversion", "-dumpversion"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout.strip()
    print("[TOOLCHAIN] CMake compiler:", actual)
    print("[TOOLCHAIN] GCC version:", version)
    print("[TOOLCHAIN] ARM nm:", nm_path)
    if objcopy_path is not None:
        print("[TOOLCHAIN] ARM objcopy:", objcopy_path)
    return str(actual), str(nm_path), (
        str(objcopy_path) if objcopy_path is not None else None
    )
