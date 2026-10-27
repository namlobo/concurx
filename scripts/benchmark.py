#!/usr/bin/env python3
"""Run repeatable ConcurX worker-count benchmarks using only Python's standard library."""

import argparse
import csv
import pathlib
import subprocess
import tempfile


def read_summary(stdout):
    rows = csv.DictReader(stdout.splitlines())
    return {row["metric"]: row["value"] for row in rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default="build/concurx", help="path to the ConcurX executable")
    parser.add_argument("--records", type=int, default=100000)
    parser.add_argument("--workers", default="1,2,4,8")
    parser.add_argument("--queue-capacity", type=int, default=256)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--output", default="benchmark.csv")
    args = parser.parse_args()
    if args.records < 0 or args.queue_capacity <= 0 or args.repeats <= 0:
        parser.error("records must be non-negative; capacity and repeats must be positive")
    try:
        workers = [int(value) for value in args.workers.split(",")]
    except ValueError:
        parser.error("--workers must be a comma-separated list of positive integers")
    if not workers or any(value <= 0 for value in workers):
        parser.error("--workers must be a comma-separated list of positive integers")

    executable = pathlib.Path(args.exe).resolve()
    if not executable.is_file():
        parser.error("executable does not exist: {}".format(executable))

    fields = [
        "input_records", "worker_count", "queue_capacity", "repeat",
        "elapsed_seconds", "throughput_records_per_second",
        "p50_latency_nanoseconds", "p95_latency_nanoseconds", "p99_latency_nanoseconds",
        "processed_records", "checksum",
    ]
    with tempfile.TemporaryDirectory(prefix="concurx-") as directory:
        input_path = pathlib.Path(directory) / "benchmark.log"
        subprocess.run(
            [str(executable), "--generate", str(args.records), "--input", str(input_path)],
            check=True, stdout=subprocess.DEVNULL,
        )
        with open(args.output, "w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=fields)
            writer.writeheader()
            for worker_count in workers:
                for repeat in range(1, args.repeats + 1):
                    completed = subprocess.run(
                        [
                            str(executable), "--input", str(input_path),
                            "--workers", str(worker_count),
                            "--queue-capacity", str(args.queue_capacity),
                        ],
                        check=True, capture_output=True, text=True,
                    )
                    summary = read_summary(completed.stdout)
                    writer.writerow({
                        "input_records": args.records,
                        "worker_count": worker_count,
                        "queue_capacity": args.queue_capacity,
                        "repeat": repeat,
                        **{field: summary[field] for field in fields[4:]},
                    })
    print("Wrote benchmark results to {}".format(args.output))


if __name__ == "__main__":
    main()
