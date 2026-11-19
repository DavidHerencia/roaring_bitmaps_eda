#!/usr/bin/env python3
"""ejecuta el benchmark ya compilado, guarda su salida y resume los resultados."""

import argparse
import datetime
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from roaring_bitmaps_eda_p.scripts.summarize_results import summarize


project_root = Path(__file__).resolve().parents[1]


def build_type(binary):
    # lee el tipo de compilacion desde la cache de cmake junto al binario
    cache_path = binary.parent / "CMakeCache.txt"
    if not cache_path.exists():
        return "unknown"
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CMAKE_BUILD_TYPE:"):
            return line.split("=", 1)[1]
    return "unknown"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=project_root / "build/release/retail_benchmark")
    parser.add_argument("--data", type=Path, default=project_root / "data/prepared/online_retail.csv")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=15)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--build-repetitions", type=int, default=3)
    parser.add_argument("--seed", type=int, default=20261115)
    parser.add_argument("--allow-debug", action="store_true", help="solo para pruebas con el fixture")
    arguments = parser.parse_args()

    binary = arguments.binary.resolve()
    data = arguments.data.resolve()
    output = arguments.output.resolve()
    if not binary.is_file():
        parser.error("benchmark binary missing; build it first")
    if not data.is_file():
        parser.error("prepared CSV missing; run scripts/prepare_data.py first")
    if output.exists() and any(output.iterdir()):
        parser.error("output must be new or empty")
    compiled_as = build_type(binary)
    if compiled_as != "Release" and not arguments.allow_debug:
        parser.error("measurement requires a CMake Release binary")

    output.mkdir(parents=True, exist_ok=True)
    command = [str(binary), "--data", str(data), "--output", str(output),
               "--repetitions", str(arguments.repetitions), "--warmup", str(arguments.warmup),
               "--build-repetitions", str(arguments.build_repetitions),
               "--seed", str(arguments.seed), "--overwrite"]
    manifest = {
        "status": "running",
        "started_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "command": command,
        "platform": platform.platform(),
        "logical_cpu_count": os.cpu_count(),
        "build_type": compiled_as,
    }
    try:
        with (output / "run.log").open("w", encoding="utf-8") as log:
            completed = subprocess.run(command, cwd=project_root, stdout=log, stderr=subprocess.STDOUT)
        if completed.returncode != 0:
            raise RuntimeError(f"benchmark exited {completed.returncode}; see {output / 'run.log'}")
        summary = summarize(output)
        manifest["status"] = "passed"
        manifest["validation"] = {
            "queries": summary["queries"],
            "query_samples": summary["query_samples"],
            "synthetic_samples": summary["synthetic_samples"],
        }
    except Exception as error:
        manifest["status"] = "failed"
        manifest["error"] = str(error)
        raise
    finally:
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"passed: {output}; {summary['query_samples']} query samples; "
          f"{summary['synthetic_samples']} synthetic samples")


if __name__ == "__main__":
    main()
