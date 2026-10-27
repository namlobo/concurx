# Testing and benchmark workflow

## Automated tests

From the project directory:

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The Visual Studio generator is multi-configuration: its executables are placed under
`build/Debug` or `build/Release`, so CTest must be given the same configuration with
`-C Debug` (or `-C Release`). Without it, CTest may report that the test is unavailable.
For a single-configuration generator such as Ninja or Unix Makefiles, the `-C` option is
usually unnecessary.

The standalone `concurx_tests` executable covers all severity values, invalid and missing
fields, blank lines, message delimiters, FIFO order, capacity blocking, consumer wakeup,
close behavior, draining, rejected pushes, and blocked producer/consumer shutdown. Its
pipeline test compares processed counts, checksum, severity totals, and component totals
for one and four workers, and checks malformed/blank classification and an empty file.

Tests coordinate blocked operations with thread-start handshakes and queue operations,
rather than relying on arbitrary sleep intervals. A failing synchronization primitive
could still cause a test thread to hang; run under an external test timeout when testing
intentionally broken queue implementations.

## CLI scenarios

```sh
build/concurx --input data/sample.log --workers 4 --queue-capacity 16 --output summary.csv
build/concurx --generate 1000 --malformed-every 10 --workers 2
```

The sample deliberately contains a blank line and a malformed line. They are reported as
separate counters and do not enter aggregate counts.

## Benchmark

After building:

```sh
python scripts/benchmark.py --exe build/concurx --records 100000 --workers 1,2,4,8 --repeats 3 --output results/benchmark.csv
```

On Windows, pass the executable path (commonly `build\\concurx.exe`) using `--exe`.
The script generates a single fixed input once, then runs each worker configuration. Each
CSV row represents one run and includes count, capacity, elapsed time, throughput,
enqueue-to-aggregate-completion latency percentiles, processed count, and checksum. Compare repetitions
and worker settings on the same machine; CPU scheduling, core count, thermal state, and
the shared aggregation lock affect results. More workers need not be faster. No benchmark
numbers are claimed until the script has actually been run on a stated machine.

Known test limitation: latency samples are retained for every successfully processed
record, which makes the percentile exact but adds memory and synchronization overhead.
