# Learning guide

Follow the code in this dependency order.

## 1. Record representation

[`LogRecord.hpp`](../include/LogRecord.hpp) holds the timestamp, severity enum, component,
message, and enqueue timestamp. The record is passed by value/move through the queue;
workers do not depend on the producer's line buffer.

## 2. Parsing and validation

[`LogParser.cpp`](../src/LogParser.cpp) separates validation from worker processing. A
result distinguishes valid records, blank lines, and malformed lines. The timestamp
format is exactly `YYYY-MM-DDTHH:MM:SS`; numeric fields, clock ranges, and calendar days
including leap years are checked.
The parser splits only the first three delimiters, so the message can contain more `|`
characters. An empty component or message is rejected.

## 3. Bounded queue

[`BoundedQueue.hpp`](../include/BoundedQueue.hpp) stores records in a deque protected by a
mutex. `unique_lock` is used because condition-variable waits temporarily release and
reacquire the mutex. Predicate waits handle spurious wakeups and recheck queue state while
holding the lock. A bounded queue provides backpressure instead of allowing input to
consume unbounded memory. Closing wakes both sides; existing records remain consumable.

## 4. Worker lifecycle

[`LogProcessor.cpp`](../src/LogProcessor.cpp) starts the configured workers before reading
the input. Each worker blocks in `pop`, processes records until the closed queue is empty,
and is joined before the method returns. A producer or worker error closes the queue;
workers are still joined before errors propagate. Without close-and-join discipline, a
producer can remain blocked forever or a thread can outlive the objects it accesses.

## 5. Aggregation

Workers call `Statistics::record_processed`; its mutex protects the severity counters,
component map, checksum, processed count, and latency vector. This is intentionally a
single straightforward critical section. Unsynchronized map updates would race and could
corrupt memory or lose counts.

## 6. Statistics

[`Statistics.cpp`](../src/Statistics.cpp) computes an FNV-1a hash per record and sums
hashes modulo 2^64, making the result deterministic regardless of worker completion order.
The latency sample begins immediately before queue submission and ends after the worker's
aggregate update completes. It includes producer backpressure, queue waiting, dispatch,
and aggregation; it is not end-to-end file-processing time or isolated CPU processing time.
Percentiles use the nearest-rank method: sort samples and select rank `ceil(p*n)`.
Throughput is processed records divided
by elapsed wall time from worker startup until all joins complete.

## 7. CLI and entry point

[`main.cpp`](../src/main.cpp) validates options, optionally generates input, runs the
processor, and emits a two-column CSV summary. `--generate N` writes `N` records and then
processes them; `--malformed-every N` intentionally makes every Nth generated line
malformed. The default generated file is `generated.log`.

## 8. Tests and benchmarks

[`tests/test_main.cpp`](../tests/test_main.cpp) exercises parsing, queue ordering/capacity,
blocking, close wakeups, draining, and deterministic aggregates at different worker
counts. [`scripts/benchmark.py`](../scripts/benchmark.py) invokes the CLI repeatedly and
collects CSV data without third-party Python packages. See [TESTING.md](./TESTING.md).
