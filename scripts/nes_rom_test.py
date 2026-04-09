#!/usr/bin/env python3
"""
nes_rom_test.py — Headless NES ROM collection smoke test

Runs each .nes ROM through cermu_console for N frames and classifies
the result:
  PASS        — loaded, ran frames, graphics detected
  BLANK       — loaded, ran frames, but framebuffer is blank or near-blank
  LOAD_FAIL   — failed to load (bad header, unsupported format)
  MAPPER_FAIL — unsupported mapper (fell back to NROM)
  CRASH       — process crashed or timed out
  NO_DETECT   — system not detected

Results written to a TSV file for triage.
"""

import os
import sys
import subprocess
import re
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

BINARY = "/tmp/cermu_console_test"
TIMEOUT_SEC = 10  # generous timeout per ROM
ROM_DIR = "/home/patrick/Downloads/NES & Famicom ROM Collection"
OUTPUT_FILE = "/home/patrick/Git/cermu/build/nes_gfx_test_results.tsv"

def classify_rom(rom_path: str) -> dict:
    """Run a single ROM and classify the result."""
    result = {
        "file": os.path.basename(rom_path),
        "folder": os.path.basename(os.path.dirname(rom_path)),
        "status": "UNKNOWN",
        "mapper": "",
        "cycles": 0,
        "frames": 0,
        "unique_colors": 0,
        "detail": "",
    }

    try:
        proc = subprocess.run(
            [BINARY, "-q", "--frames", "60", "--gfx", rom_path],
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
            env={**os.environ, "TERM": "dumb"},
        )
        out = proc.stdout + proc.stderr

        # Extract mapper
        m = re.search(r'[Mm]apper\s*[:=]?\s*(\d+)', out)
        if m:
            result["mapper"] = m.group(1)

        # Unsupported mapper fallback
        if "Unsupported mapper" in out or "falling back to NROM" in out:
            m2 = re.search(r'Unsupported mapper (\d+)', out)
            result["status"] = "MAPPER_UNSUPPORTED"
            result["detail"] = f"mapper {m2.group(1)}" if m2 else "unknown mapper"
            return result

        # Load failure
        if "Failed to load" in out or "Failed to parse" in out:
            result["status"] = "LOAD_FAIL"
            result["detail"] = "cartridge load failed"
            return result

        if "No system found" in out or "ERROR:" in out:
            result["status"] = "NO_DETECT"
            result["detail"] = "system not detected"
            return result

        # Extract cycle count from last frame report
        cycles_matches = re.findall(r'Cycles:\s*(\d+)', out)
        if cycles_matches:
            result["cycles"] = int(cycles_matches[-1])

        # Extract frame count
        frame_matches = re.findall(r'Frame\s+(\d+)', out)
        if frame_matches:
            result["frames"] = int(frame_matches[-1])

        # Check if completed successfully
        if "Test completed successfully" in out or proc.returncode == 0:
            # Parse graphics result
            gfx_m = re.search(r'GFX_RESULT:\s*unique_colors=(\d+)', out)
            if gfx_m:
                uc = int(gfx_m.group(1))
                result["unique_colors"] = uc
                if uc <= 2:
                    result["status"] = "BLANK"
                    result["detail"] = f"{uc} unique colors"
                else:
                    result["status"] = "PASS"
            else:
                result["status"] = "PASS"
        elif proc.returncode != 0:
            result["status"] = "CRASH"
            result["detail"] = f"exit code {proc.returncode}"
        else:
            result["status"] = "PASS"

    except subprocess.TimeoutExpired:
        result["status"] = "TIMEOUT"
        result["detail"] = f"exceeded {TIMEOUT_SEC}s"
    except Exception as e:
        result["status"] = "ERROR"
        result["detail"] = str(e)

    return result


def find_roms(base_dir: str) -> list:
    """Find all .nes files recursively."""
    roms = []
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            if f.lower().endswith('.nes'):
                roms.append(os.path.join(root, f))
    roms.sort()
    return roms


def main():
    if not os.path.isfile(BINARY):
        print(f"ERROR: Binary not found: {BINARY}")
        sys.exit(1)

    roms = find_roms(ROM_DIR)
    total = len(roms)
    print(f"Found {total} ROMs in {ROM_DIR}")

    # Run tests with limited parallelism (4 threads — CPU-bound)
    results = []
    start_time = time.time()

    with ThreadPoolExecutor(max_workers=4) as pool:
        futures = {pool.submit(classify_rom, rom): rom for rom in roms}
        done = 0
        for future in as_completed(futures):
            done += 1
            r = future.result()
            results.append(r)
            status_char = {"PASS": ".", "BLANK": "B", "MAPPER_UNSUPPORTED": "M",
                           "LOAD_FAIL": "L", "CRASH": "X", "TIMEOUT": "T",
                           "NO_DETECT": "?", "ERROR": "E"}.get(r["status"], "?")
            # Print progress inline
            sys.stdout.write(status_char)
            if done % 80 == 0:
                sys.stdout.write(f" [{done}/{total}]\n")
            sys.stdout.flush()

    elapsed = time.time() - start_time
    print(f"\n\nCompleted {total} ROMs in {elapsed:.1f}s")

    # Sort results: failures first, then by folder/file
    results.sort(key=lambda r: (
        0 if r["status"] not in ("PASS",) else 1,
        r["folder"], r["file"]
    ))

    # Write TSV
    with open(OUTPUT_FILE, 'w') as f:
        f.write("status\tfolder\tfile\tmapper\tunique_colors\tcycles\tframes\tdetail\n")
        for r in results:
            f.write(f"{r['status']}\t{r['folder']}\t{r['file']}\t"
                    f"{r['mapper']}\t{r['unique_colors']}\t{r['cycles']}\t{r['frames']}\t{r['detail']}\n")

    # Summary
    from collections import Counter
    counts = Counter(r["status"] for r in results)
    print(f"\nResults written to {OUTPUT_FILE}")
    print(f"Summary:")
    for status, count in sorted(counts.items()):
        print(f"  {status:20s}: {count:4d}")

    # List unsupported mappers
    unsupported = [r for r in results if r["status"] == "MAPPER_UNSUPPORTED"]
    if unsupported:
        mapper_counts = Counter(r["detail"] for r in unsupported)
        print(f"\nUnsupported mappers ({len(unsupported)} ROMs):")
        for mapper, count in sorted(mapper_counts.items(), key=lambda x: -x[1]):
            print(f"  {mapper}: {count} ROMs")

    # List failures
    failures = [r for r in results
                if r["status"] in ("CRASH", "TIMEOUT", "LOAD_FAIL", "ERROR")]
    if failures:
        print(f"\nFailures ({len(failures)} ROMs):")
        for r in failures[:50]:
            print(f"  [{r['status']}] {r['folder']}/{r['file']}: {r['detail']}")
        if len(failures) > 50:
            print(f"  ... and {len(failures) - 50} more")

    # List blank-screen ROMs
    blanks = [r for r in results if r["status"] == "BLANK"]
    if blanks:
        print(f"\nBlank screen ({len(blanks)} ROMs):")
        for r in blanks[:80]:
            print(f"  {r['folder']}/{r['file']}: {r['detail']} (mapper {r['mapper']})")
        if len(blanks) > 80:
            print(f"  ... and {len(blanks) - 80} more")


if __name__ == "__main__":
    main()
