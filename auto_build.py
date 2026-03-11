#!/usr/bin/env python3

# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring
# pylint: disable=broad-exception-caught
# pylint: disable=import-outside-toplevel
# pylint: disable=too-many-branches
# pylint: disable=too-many-statements

from typing import NoReturn
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import platform

def die(msg: str) -> NoReturn:
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(1)

def run(cmd, cwd=None, quiet=False):
    if not quiet:
        loc = f"(cd {cwd}) " if cwd else ""
        print(f"{loc}{cmd}")

    proc = subprocess.run(
        cmd,
        cwd=cwd,
        shell=True,
        stdout=subprocess.PIPE if quiet else None,
        stderr=subprocess.PIPE if quiet else None,
        text=True,
        check=False,
    )

    if proc.returncode != 0:
        if quiet:
            print(proc.stdout or "")
            print(proc.stderr or "")
        die(f"Command failed: {cmd}")

    return proc

def execute_in_dir(dirpath: Path | str, command: str, quiet: bool):
    path = Path(dirpath)
    if not path.is_dir():
        die(f"Directory {dirpath} does not exist")
    run(command, cwd=dirpath, quiet=quiet)

def get_module_dir(flag: str) -> str:
    if flag.startswith("CUSTOM_MUTATOR_"):
        return "custommutator"
    if flag == "BITCOIN_CORE":
        return "modules/bitcoin"
    return f"modules/{flag.lower().replace('_', '')}"

def should_build_sequentially(flag: str) -> bool:
    return flag in {"SECP256K1", "BITCOINJ", "LIGHTNING_KMP"} or flag.startswith(
        "CUSTOM_MUTATOR_"
    )

def get_flags(cxxflags: str):
    return re.findall(r"-D([A-Z0-9_]+)", cxxflags)

# Maps module flags to required git submodule paths defined in .gitmodules.
SUBMODULES_BY_FLAG = {
    "CLIGHTNING": ["external/lightning"],
    "SECP256K1": ["external/secp256k1"],
    "ECLAIR": ["external/eclair"],
    "LIBWALLY_CORE": ["external/libwally-core"],
    "BITCOIN_CORE": ["external/bitcoin-core"],
}

def ensure_submodules_for(flag: str, quiet: bool):
    paths = SUBMODULES_BY_FLAG.get(flag, [])
    if not paths:
        return
    if not quiet:
        print(f"Ensuring submodules for {flag}: {', '.join(paths)}")
    for path in paths:
        run(f"git submodule update --init --recursive {path}", quiet=quiet)

def full_clean():
    print("Performing full clean...")
    for d in Path("modules").glob("*/"):
        execute_in_dir(d, "make clean", quiet=False)
    execute_in_dir("custommutator", "make clean", quiet=False)

def clean_by_flags(flags):
    print(f"Cleaning modules: {' '.join(flags)}")
    for flag in flags:
        execute_in_dir(get_module_dir(flag), "make clean", quiet=False)

def build_module(flag: str, quiet: bool):
    dirpath = get_module_dir(flag)

    if not quiet:
        print(f"Building module: {flag}")

    ensure_submodules_for(flag, quiet)
    execute_in_dir(dirpath, "make", quiet)

    if quiet:
        print(f"✓ {flag} built successfully")


def main():
    cxxflags = os.environ.get("CXXFLAGS")
    if not cxxflags:
        die('CXXFLAGS not defined. Example: CXXFLAGS="-DLDK -DLND" ./auto_build.py')

    print("Cleaning previous builds...")
    run("make clean")

    clean_build = os.environ.get("CLEAN_BUILD")

    if clean_build:
        if clean_build == "FULL":
            full_clean()
        elif clean_build == "CLEAN":
            clean_by_flags(get_flags(cxxflags))
        else:
            clean_by_flags(clean_build.split())
    else:
        print("No CLEAN_BUILD option specified. Skipping clean step.")

    flags = get_flags(cxxflags)
    if not flags:
        print("No modules to build.")
        return

    parallel_jobs = int(os.environ.get("PARALLEL_JOBS", "0"))
    quiet = parallel_jobs != 1

    if parallel_jobs == 1:
        print(f"Compiling selected modules sequentially with CXXFLAGS={cxxflags}...")
    else:
        print(
            f"Compiling selected modules in parallel (jobs={parallel_jobs}) "
            f"with CXXFLAGS={cxxflags}..."
        )

    sequential = [f for f in flags if should_build_sequentially(f)]
    parallel = [f for f in flags if not should_build_sequentially(f)]

    seq_error = None

    # Sequential group (runs in background)
    if sequential:

        def run_sequential():
            nonlocal seq_error
            print(f"Starting sequential module builds:{' '.join(sequential)}")
            try:
                for f in sequential:
                    build_module(f, quiet)
            except BaseException as e:
                # Captures SystemExit raised by die()'s sys.exit()
                seq_error = e

        import threading

        seq_thread = threading.Thread(target=run_sequential)
        seq_thread.start()

    # Parallel builds
    if parallel:
        with ThreadPoolExecutor(max_workers=parallel_jobs or None) as pool:
            futures = {pool.submit(build_module, f, quiet): f for f in parallel}
            try:
                for fut in as_completed(futures):
                    fut.result()
            except Exception:
                if sequential:
                    print("Parallel build failed, terminating sequential builds")
                die("One or more module builds failed")

    if sequential:
        seq_thread.join()
        if seq_error:
            # Re-raise/abort in the main thread so the script actually fails.
            die("Sequential module builds failed")

    print("All module builds completed successfully!")

    only_modules = int(os.environ.get("ONLY_MODULES", "0"))
    if not only_modules:
        print("Compiling the main project in the root...")
        run("make")

    print("Build completed successfully!")


if __name__ == "__main__":
    main()
