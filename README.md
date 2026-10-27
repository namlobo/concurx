# ConcurX — Concurrent Data Processing Engine

ConcurX is a standalone, in-process C++17 systems project. It parses structured log
records on one producer thread, passes valid records through a bounded synchronized
queue, and aggregates them across configurable worker threads. It is not a distributed
streaming platform.

## Features

- C++17, CMake, standard library threads and synchronization
- Predicate-based bounded producer-consumer queue with close and drain semantics
- Strict severity and timestamp-shape validation; malformed and blank lines are counted
- Concurrent severity/component counts, enqueue-to-completion latency percentiles, and a
  deterministic consistency checksum
- Synthetic input generation and repeatable Python standard-library benchmarking
- Standalone automated tests with no third-party test framework

## Architecture

`input file -> single producer/parser -> bounded queue -> worker threads -> synchronized
statistics -> CSV summary`

The queue blocks the producer at capacity and blocks consumers when empty. Closing wakes
both sides and allows consumers to drain queued records. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/LEARNING_GUIDE.md](docs/LEARNING_GUIDE.md) for synchronization and code walkthroughs.

## Prerequisites and build

Requirements: a C++17 compiler, CMake 3.10 or newer, and Python 3 for the optional
benchmark. No external runtime or test dependencies are required.

```sh
cmake -S . -B build
cmake --build build --config Debug
```

For the Visual Studio generator, run tests with the configuration used to build:

```sh
ctest --test-dir build -C Debug --output-on-failure
```

Visual Studio is a multi-configuration generator and stores its test executable under
`build/Debug` (or `build/Release`). Single-configuration generators generally do not need
the `-C` option.

## CLI

```text
concurx --input <path> [--workers N] [--queue-capacity N] [--output <path>]
concurx --generate N [--input <path>] [--malformed-every N]
        [--workers N] [--queue-capacity N] [--output <path>]
concurx --help
```

Defaults are hardware concurrency (or one worker if unavailable) and queue capacity 256.
Both must be positive. Generate mode writes to the provided `--input` path, or
`generated.log` if omitted, then processes that file. `--malformed-every N` emits a
deliberately malformed record on every Nth generated line. Summary CSV is written to
stdout and also to `--output` when provided; errors go to stderr with exit status 2.

```sh
build/concurx --input data/sample.log --workers 4 --queue-capacity 256 --output summary.csv
build/concurx --generate 10000 --malformed-every 100 --workers 4
```

Windows users can invoke `build\\concurx.exe` (or `build\\Debug\\concurx.exe` depending on
the generator).

## Input and metrics

Records use `timestamp|level|component|message`, for example
`2026-10-27T10:15:30|INFO|scheduler|Task started`. Timestamp shape is exactly
`YYYY-MM-DDTHH:MM:SS` with checked numeric fields and basic ranges. Severity must be
`DEBUG`, `INFO`, `WARN`, or `ERROR`. Component and message must be non-empty. The first
three pipes delimit fields; additional pipes remain in the message.

Every input line increments `total_input_records`. Whitespace-only lines increment
`blank_lines`; invalid nonblank lines increment `malformed_records`; only valid queued and
processed records contribute to severity/component aggregates and checksum. Latency is
measured from just before queue submission until a worker completes its aggregate update;
it includes producer backpressure, queue waiting, dispatch, and aggregation.
Elapsed time covers processing from worker startup through worker joins. Throughput is
processed records divided by elapsed wall time. p50/p95/p99 use nearest-rank percentiles.
The checksum sums each record's 64-bit FNV-1a hash modulo 2^64; it is order-independent
and not cryptographic.

## Tests and benchmarks

```sh
ctest --test-dir build -C Debug --output-on-failure
python scripts/benchmark.py --exe build/concurx --records 100000 --workers 1,2,4,8 --repeats 3 --output results/benchmark.csv
```

The benchmark runs the same generated input across worker configurations and records each
repetition. It does not assume additional workers improve throughput. See
[docs/TESTING.md](docs/TESTING.md) for details.

## Limitations and learning goals

This teaching implementation has one producer, a shared statistics mutex, and stores an
exact latency sample for each processed record. These choices favor understandable
correctness over maximum scale. They can limit throughput or increase memory use. The
project demonstrates RAII, thread lifecycle management, condition variables, backpressure,
safe shared-state aggregation, deterministic correctness checks, testing, and benchmarking.
Use [docs/INTERVIEW_QA.md](docs/INTERVIEW_QA.md) to review concurrency concepts and the
implemented design.