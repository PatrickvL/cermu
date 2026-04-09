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

Usage:
  nes_rom_test.py                  # run full suite
  nes_rom_test.py --quick          # 60 frames, fast triage
  nes_rom_test.py --blanks-only    # re-test only previously blank ROMs
  nes_rom_test.py --changed-only   # re-test only non-PASS ROMs (BLANK/CRASH/etc.)
  nes_rom_test.py --mapper 210     # test only a specific mapper
  nes_rom_test.py --folder "NES North America ROMs"  # test one subfolder
  nes_rom_test.py --early-exit     # stop each ROM as soon as video output detected
"""

import os
import sys
import subprocess
import re
import time
import argparse
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

BINARY = "/tmp/cermu_console_test"
TIMEOUT_SEC = 30  # per-ROM timeout (must cover 300 frames of emulation)
ROM_DIR = "/home/patrick/Downloads/NES & Famicom ROM Collection"
OUTPUT_FILE = "/home/patrick/Git/cermu/build/nes_gfx_test_results.tsv"
DEFAULT_FRAMES = 300

# ROMs known to hang or take excessively long — skip to save test time.
# Format: exact filename (basename).  Add entries as discovered.
KNOWN_TIMEOUT_ROMS = {
    "Days of Thunder (North America).nes",
}

def read_mapper_from_header(rom_path: str) -> str:
    """Extract iNES mapper ID directly from the ROM header."""
    try:
        with open(rom_path, 'rb') as f:
            header = f.read(16)
        if len(header) < 16 or header[:4] != b'NES\x1a':
            return ""
        byte6 = header[6]
        byte7 = header[7]
        byte8 = header[8]
        is_nes20 = (byte7 & 0x0C) == 0x08
        # Check for archaic header (garbage in bytes 12-15)
        archaic = not is_nes20 and any(header[12:16])
        if archaic:
            return str(byte6 >> 4)
        if is_nes20:
            mapper = ((byte8 & 0x0F) << 8) | (byte7 & 0xF0) | (byte6 >> 4)
        else:
            mapper = (byte7 & 0xF0) | (byte6 >> 4)
        return str(mapper)
    except Exception:
        return ""


def classify_rom(rom_path: str, frames: int = DEFAULT_FRAMES, early_exit: bool = False) -> dict:
    """Run a single ROM and classify the result."""
    basename = os.path.basename(rom_path)
    result = {
        "file": basename,
        "folder": os.path.basename(os.path.dirname(rom_path)),
        "status": "UNKNOWN",
        "mapper": read_mapper_from_header(rom_path),
        "cycles": 0,
        "frames": 0,
        "unique_colors": 0,
        "detail": "",
    }

    # Skip known timeout ROMs
    if basename in KNOWN_TIMEOUT_ROMS:
        result["status"] = "TIMEOUT"
        result["detail"] = "known timeout (skipped)"
        return result

    try:
        cmd = [BINARY, "-q", "--frames", str(frames), "--gfx"]
        if early_exit:
            cmd.append("--early-exit")
        cmd.append(rom_path)
        proc = subprocess.run(
            cmd,
            capture_output=True, text=True,
            timeout=TIMEOUT_SEC,
            env={**os.environ, "TERM": "dumb"},
        )
        out = proc.stdout + proc.stderr

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
            gfx_m = re.search(r'GFX_RESULT:\s*unique_colors=(\d+)\s+visible_pixels=(\d+)\s+frames_ran=(\d+)', out)
            if not gfx_m:
                gfx_m = re.search(r'GFX_RESULT:\s*unique_colors=(\d+)', out)
            if gfx_m:
                uc = int(gfx_m.group(1))
                result["unique_colors"] = uc
                if gfx_m.lastindex and gfx_m.lastindex >= 3:
                    result["frames"] = int(gfx_m.group(3))
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
    parser = argparse.ArgumentParser(description="NES ROM smoke test")
    parser.add_argument("--quick", action="store_true",
                        help="Run 60 frames instead of 300 (fast triage)")
    parser.add_argument("--frames", type=int, default=None,
                        help="Override frame count")
    parser.add_argument("--blanks-only", action="store_true",
                        help="Re-test only ROMs that were BLANK in previous run")
    parser.add_argument("--changed-only", action="store_true",
                        help="Re-test only non-PASS ROMs (BLANK, CRASH, TIMEOUT, etc.)")
    parser.add_argument("--mapper", type=str, default=None,
                        help="Test only ROMs using a specific mapper ID")
    parser.add_argument("--folder", type=str, default=None,
                        help="Test only ROMs in a specific subfolder")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of parallel workers (default: 4)")
    parser.add_argument("--early-exit", action="store_true",
                        help="Stop each ROM as soon as video output is detected")
    args = parser.parse_args()

    frames = args.frames or (60 if args.quick else DEFAULT_FRAMES)

    if not os.path.isfile(BINARY):
        print(f"ERROR: Binary not found: {BINARY}")
        print(f"       Run: cp build/bin/cermu_console {BINARY}")
        sys.exit(1)

    roms = find_roms(ROM_DIR)

    # Filter by folder
    if args.folder:
        roms = [r for r in roms if args.folder in os.path.basename(os.path.dirname(r))]
        if not roms:
            print(f"No ROMs found matching folder '{args.folder}'")
            sys.exit(1)

    # Filter: blanks-only from previous results
    prev_blanks = set()
    if args.blanks_only and os.path.isfile(OUTPUT_FILE):
        with open(OUTPUT_FILE) as f:
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 3 and parts[0] == "BLANK":
                    prev_blanks.add(parts[2])  # filename
        roms = [r for r in roms if os.path.basename(r) in prev_blanks]
        if not roms:
            print("No previously blank ROMs found")
            sys.exit(0)
        print(f"Re-testing {len(roms)} previously blank ROMs")

    # Filter: changed-only — re-test everything that isn't PASS
    if args.changed_only and os.path.isfile(OUTPUT_FILE):
        non_pass = set()
        with open(OUTPUT_FILE) as f:
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 3 and parts[0] not in ("PASS", "status"):
                    non_pass.add(parts[2])  # filename
        roms = [r for r in roms if os.path.basename(r) in non_pass]
        if not roms:
            print("All ROMs already PASS — nothing to re-test")
            sys.exit(0)
        print(f"Re-testing {len(roms)} non-PASS ROMs")

    # Filter: specific mapper (requires previous results for mapper column)
    if args.mapper and os.path.isfile(OUTPUT_FILE):
        mapper_files = set()
        with open(OUTPUT_FILE) as f:
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 4 and parts[3] == args.mapper:
                    mapper_files.add(parts[2])
        roms = [r for r in roms if os.path.basename(r) in mapper_files]
        if not roms:
            print(f"No ROMs found for mapper {args.mapper}")
            sys.exit(1)
        print(f"Testing {len(roms)} mapper {args.mapper} ROMs")

    total = len(roms)
    print(f"Testing {total} ROMs ({frames} frames, {args.workers} workers)")

    # Run tests with limited parallelism
    results = []
    start_time = time.time()

    use_early_exit = args.early_exit
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(classify_rom, rom, frames, use_early_exit): rom for rom in roms}
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

    # Write TSV — merge into existing results if running a subset
    output_file = OUTPUT_FILE
    existing = {}
    is_subset = (args.blanks_only or args.changed_only or args.mapper or args.folder)
    if is_subset and os.path.isfile(output_file):
        with open(output_file) as f:
            header_line = f.readline()
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) >= 3:
                    existing[parts[2]] = line.strip()  # key by filename
        # Merge: update existing entries with new results
        for r in results:
            existing[r['file']] = (f"{r['status']}\t{r['folder']}\t{r['file']}\t"
                                   f"{r['mapper']}\t{r['unique_colors']}\t{r['cycles']}\t{r['frames']}\t{r['detail']}")
        with open(output_file, 'w') as f:
            f.write("status\tfolder\tfile\tmapper\tunique_colors\tcycles\tframes\tdetail\n")
            for line in sorted(existing.values()):
                f.write(line + '\n')
    else:
        with open(output_file, 'w') as f:
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
        # Group by mapper for clean output
        mapper_groups = {}
        for r in blanks:
            mid = r["mapper"] or "?"
            mapper_groups.setdefault(mid, []).append(r)
        for mid in sorted(mapper_groups, key=lambda m: -len(mapper_groups[m])):
            group = mapper_groups[mid]
            print(f"  Mapper {mid} ({len(group)}):")
            for r in group[:10]:
                print(f"    {r['folder']}/{r['file']}")
            if len(group) > 10:
                print(f"    ... and {len(group) - 10} more")


if __name__ == "__main__":
    main()
