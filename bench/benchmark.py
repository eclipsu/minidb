#!/usr/bin/env python3
"""
MiniDB end-to-end benchmark.

Run from the repo root:
    python3 bench/benchmark.py                 # sizes 1,000 / 10,000 / 100,000
    python3 bench/benchmark.py --sizes 1000 5000 --runs 3

What it does:
  1. Builds an OPTIMIZED copy of MiniDB (-O2) as ./minidb_bench
     (the normal Makefile build has no optimization, so timings would be 5-10x too slow).
  2. For each table size N, generates SQL scripts and pipes them into minidb_bench:
       - N INSERTs in random order (the baseline)
       - the baseline + OPS point lookups  (SELECT ... WHERE id = k)
       - the baseline + OPS updates        (UPDATE ... WHERE id = k)
       - the baseline + OPS deletes        (DELETE ... WHERE id = k)
     The cost of an operation = (time with those OPS) - (baseline time), divided by OPS.
     Each script runs several times and the FASTEST run is used (the least disturbed one).
     If the extra time is too small to measure reliably, the result is marked "noisy".
  3. Prints a table of microseconds per operation.

Everything goes through the real program: tokenizer -> parser -> executor -> B-Tree.
"""

import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BINARY = REPO / "minidb_bench"


def build():
    sources = sorted(str(p) for p in (REPO / "src").glob("*.cpp"))
    if not sources:
        sys.exit(f"No src/*.cpp found under {REPO}. Run this from the minidb repo.")
    cmd = ["g++", "-std=c++17", "-O2", f"-I{REPO}", "-o", str(BINARY)] + sources
    print("Building optimized binary:", " ".join(cmd[:5]), "... src/*.cpp")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("Build failed:\n" + result.stderr)


def make_inserts(n, rng):
    ids = list(range(1, n + 1))
    rng.shuffle(ids)
    lines = ["CREATE TABLE users (id INT, name TEXT, age INT);"]
    lines += [f"INSERT INTO users VALUES ({i}, 'user{i}', {i % 90});" for i in ids]
    return lines


def make_ops(kind, n, ops, rng):
    keys = rng.sample(range(1, n + 1), min(ops, n))
    if kind == "lookup":
        return [f"SELECT * FROM users WHERE id = {k};" for k in keys]
    if kind == "update":
        return [f"UPDATE users SET age = {k % 77} WHERE id = {k};" for k in keys]
    if kind == "delete":
        return [f"DELETE FROM users WHERE id = {k};" for k in keys]
    raise ValueError(kind)


def run_script(lines, workdir):
    """Pipe SQL into minidb_bench. Returns (seconds, number of error lines in the output)."""
    # ".exit" then "discard": if the program asks "save, discard or cancel?" on exit,
    # we answer discard; if it doesn't ask, it has already quit and ignores the rest.
    script = "\n".join(lines + [".exit", "discard"]) + "\n"
    start = time.perf_counter()
    result = subprocess.run([str(BINARY)], input=script, capture_output=True, text=True, cwd=workdir)
    seconds = time.perf_counter() - start
    out = result.stdout + result.stderr
    errors = sum(1 for line in out.splitlines() if "error" in line.lower())
    return seconds, errors


def timed(lines, runs, workdir, label):
    """Fastest of `runs` runs: noise (other programs, caches) only ever makes a run slower."""
    times = []
    for _ in range(runs):
        seconds, errors = run_script(lines, workdir)
        if errors:
            print(f"  WARNING: {label}: MiniDB printed {errors} error line(s). "
                  f"Check that it accepts this SQL syntax.")
        times.append(seconds)
    return min(times)


def main():
    ap = argparse.ArgumentParser(description="MiniDB end-to-end benchmark")
    ap.add_argument("--sizes", type=int, nargs="+", default=[1000, 10000, 100000],
                    help="table sizes to test (default: 1000 10000 100000)")
    ap.add_argument("--ops", type=int, default=None,
                    help="lookups/updates/deletes per size (default: half the rows, at least 2000, "
                         "so the ops take long enough to measure next to the load time)")
    ap.add_argument("--runs", type=int, default=5, help="repeat each run, keep the fastest (default: 5)")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--no-build", action="store_true", help="reuse an existing ./minidb_bench")
    args = ap.parse_args()

    if not args.no_build:
        build()
    elif not BINARY.exists():
        sys.exit("./minidb_bench doesn't exist yet: run without --no-build first.")

    workdir = tempfile.mkdtemp(prefix="minidb_bench_")
    rows = []
    try:
        for n in args.sizes:
            print(f"\nN = {n:,} rows ...")
            rng = random.Random(args.seed + n)
            base = make_inserts(n, rng)
            t_base = timed(base, args.runs, workdir, f"{n} inserts")

            per_op = {}
            for kind in ("lookup", "update", "delete"):
                count = args.ops if args.ops else max(2000, n // 2)
                ops = make_ops(kind, n, count, random.Random(args.seed + n + len(kind)))
                t = timed(base + ops, args.runs, workdir, f"{n} + {kind}s")
                extra = t - t_base
                # The ops must add clearly more time than the run-to-run noise of the baseline.
                if extra < 0.03 * t_base or extra <= 0:
                    per_op[kind] = None
                    print(f"  {kind:<7}      noisy  (ops added {extra*1000:.1f} ms to a {t_base:.2f}s run: use more --ops)")
                else:
                    per_op[kind] = extra / len(ops) * 1e6
                    print(f"  {kind:<7} {per_op[kind]:10.1f} µs/op")

            rows.append((n, t_base, t_base / n * 1e6, per_op["lookup"], per_op["update"], per_op["delete"]))
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    ops_note = f"{args.ops} ops per kind" if args.ops else "ops per kind = half the rows (min 2000)"
    print(f"\nResults (fastest of {args.runs} runs, {ops_note}, -O2 build)\n")
    header = f"{'rows':>10} | {'load total':>10} | {'us/insert':>9} | {'us/lookup':>9} | {'us/update':>9} | {'us/delete':>9}"
    print(header)
    print("-" * len(header))
    cell = lambda v: f"{'noisy':>9}" if v is None else f"{v:>9.1f}"
    for n, total, ins, look, upd, dele in rows:
        print(f"{n:>10,} | {total:>9.2f}s | {ins:>9.1f} | {cell(look)} | {cell(upd)} | {cell(dele)}")

    print("\nHow to read it:")
    print("  µs/insert should grow only slowly as rows grow 10x: that's the B-Tree's O(log n).")
    print("  If µs/lookup grows ~10x when rows grow 10x, the executor is scanning every row for")
    print("  WHERE id = k (O(n)) instead of using tree.find() on the key (O(log n)).")


if __name__ == "__main__":
    main()