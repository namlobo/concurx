# Interview questions and answers

1. **What is a race condition?** A result depends on uncontrolled timing between threads.
   Concurrent unsynchronized writes to the same object are data races and undefined behavior.
2. **What is a critical section?** Code accessing shared state that must be mutually
   exclusive; here, statistics updates and queue operations are protected sections.
3. **Mutex vs. semaphore?** A mutex grants exclusive ownership; a semaphore manages a
   count of permits. This queue uses a mutex plus condition variables rather than a semaphore.
4. **Mutex vs. atomic?** A mutex can protect compound invariants and containers. An atomic
   provides indivisible operations on a value, but does not make map updates safe.
5. **`lock_guard` vs. `unique_lock`?** `lock_guard` is simple scope-bound locking.
   `unique_lock` can be unlocked/relocked and is required by condition-variable waits.
6. **Why use a condition variable?** To suspend a thread until shared state may satisfy a
   condition, without busy-waiting.
7. **What is a spurious wakeup?** A wait can return without the desired condition becoming
   true. The queue rechecks its predicate.
8. **Why predicate-based waits?** They correctly handle spurious wakeups and state changes
   that happen before the waiter resumes.
9. **What is producer-consumer synchronization?** Coordinating producers adding items and
   consumers removing them while preserving queue invariants.
10. **Why a bounded queue?** It caps memory usage and applies backpressure to a faster producer.
11. **What is backpressure?** Slowing/restricting upstream work when downstream capacity is full.
12. **How can deadlocks occur?** Threads can wait cyclically for locks or conditions that
   cannot become true. This queue uses one mutex and close wakes blocked queue operations.
13. **What is lock contention?** Multiple threads competing to acquire the same lock,
   reducing parallel progress; statistics aggregation can contend.
14. **What is false sharing?** Independent frequently modified values share a cache line,
   causing cache-coherence traffic. This implementation does not add padded per-worker counters.
15. **What is thread safety?** Correct behavior when operations are invoked concurrently;
   queue state and shared statistics are mutex-protected.
16. **How does graceful shutdown work?** Close rejects new pushes, wakes waiters, consumers
   drain queued items, and the caller joins every worker before returning.
17. **How prevent lost or duplicate records?** One producer pushes each valid line once;
   each successful pop is processed once; processed count and deterministic checksum are
   compared across worker configurations in tests.
18. **Why can more workers reduce performance?** Scheduling, queue contention, aggregation
   contention, memory bandwidth, or limited CPU cores can dominate useful parallel work.
19. **How are throughput and latency measured?** Throughput is processed records over
   full pipeline wall time. Latency runs from immediately before enqueue until the worker
   completes the aggregate update; these are distinct metrics.
20. **How profile this application?** Build with debug symbols, run a representative input,
   and use a platform profiler to identify time in parsing, synchronization, allocation,
   or worker scheduling.
21. **How test concurrent code?** Use deterministic invariants, controlled synchronization,
   repeated runs, timeouts, and compare aggregates with a single-threaded baseline.
22. **What are architecture limitations?** One producer, a shared aggregation mutex, and
   exact per-record latency retention can constrain throughput and memory.
23. **What does the checksum prove?** It is a deterministic consistency signal, not a
   collision-proof or cryptographic proof of correctness.
24. **What improvements are not implemented?** Sharded aggregation, sampled latency,
   multiple producers, and lock-free structures could be explored but are not part of this code.
