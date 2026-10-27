#include "BoundedQueue.hpp"
#include "LogParser.hpp"
#include "LogProcessor.hpp"

#include <atomic>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

void test_parser() {
    concurx::LogParser parser;
    const char* levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    for (int i = 0; i < 4; ++i) {
        const concurx::ParseResult result =
            parser.parse(std::string("2026-10-27T10:15:30|") + levels[i] + "|core|message|tail");
        check(result.success, std::string("valid level ") + levels[i]);
        check(result.record.message == "message|tail", "message retains additional delimiters");
    }
    check(parser.parse("   \t").blank, "whitespace-only line is blank");
    check(!parser.parse("bad").success, "record with missing delimiters is malformed");
    check(!parser.parse("2026-10-27T10:15:30|TRACE|core|message").success,
          "unsupported severity is rejected");
    check(!parser.parse("2026-13-27T10:15:30|INFO|core|message").success,
          "invalid timestamp fields are rejected");
    check(!parser.parse("2026-02-30T10:15:30|INFO|core|message").success,
          "invalid calendar dates are rejected");
    check(!parser.parse("2026-10-27T10:15:30|INFO||message").success,
          "empty component is rejected");
    check(!parser.parse("2026-10-27T10:15:30|INFO|core|").success,
          "empty message is rejected");
}

void test_queue() {
    concurx::BoundedQueue<int> queue(2);
    check(queue.push(1) && queue.push(2), "queue accepts items within capacity");
    std::atomic<bool> producer_started(false);
    std::atomic<bool> producer_finished(false);
    bool producer_result = false;
    std::thread producer([&] {
        producer_started.store(true);
        producer_result = queue.push(3);
        producer_finished.store(true);
    });
    while (!producer_started.load()) std::this_thread::yield();
    check(!producer_finished.load(), "producer waits while queue is full");
    int item = 0;
    check(queue.pop(item) && item == 1, "queue preserves FIFO order");
    producer.join();
    check(producer_result, "blocked producer resumes when an item is removed");
    check(queue.pop(item) && item == 2, "second item is FIFO");
    check(queue.pop(item) && item == 3, "producer item is available after unblock");

    std::atomic<bool> consumer_started(false);
    bool consumer_result = false;
    int consumed = 0;
    std::thread consumer([&] {
        consumer_started.store(true);
        consumer_result = queue.pop(consumed);
    });
    while (!consumer_started.load()) std::this_thread::yield();
    check(queue.push(4), "push wakes waiting consumer");
    consumer.join();
    check(consumer_result && consumed == 4, "consumer receives notified item");

    check(queue.push(5), "queue accepts item before close");
    queue.close();
    check(!queue.push(6), "closed queue rejects pushes");
    check(queue.pop(item) && item == 5, "closed queue drains pending items");
    check(!queue.pop(item), "closed and empty queue ends consumer");

    concurx::BoundedQueue<int> waiting_queue(1);
    std::atomic<bool> waiting_consumer_started(false);
    bool waiting_consumer_result = true;
    std::thread waiting_consumer([&] {
        waiting_consumer_started.store(true);
        int unused = 0;
        waiting_consumer_result = waiting_queue.pop(unused);
    });
    while (!waiting_consumer_started.load()) std::this_thread::yield();
    waiting_queue.close();
    waiting_consumer.join();
    check(!waiting_consumer_result, "close wakes a consumer waiting on an empty queue");

    concurx::BoundedQueue<int> waiting_producer_queue(1);
    check(waiting_producer_queue.push(1), "fill queue before producer shutdown test");
    std::atomic<bool> waiting_producer_started(false);
    bool waiting_producer_result = true;
    std::thread waiting_producer([&] {
        waiting_producer_started.store(true);
        waiting_producer_result = waiting_producer_queue.push(2);
    });
    while (!waiting_producer_started.load()) std::this_thread::yield();
    waiting_producer_queue.close();
    waiting_producer.join();
    check(!waiting_producer_result, "close wakes a producer waiting on a full queue");
}

concurx::StatisticsSnapshot run_processor(std::size_t worker_count, const std::string& path) {
    concurx::LogProcessor processor = concurx::LogProcessor({worker_count, 3});
    return processor.process_file(path);
}

void test_processor() {
    const std::string path = "concurx_test_input.log";
    {
        std::ofstream output(path.c_str());
        output << "2026-10-27T10:15:30|INFO|scheduler|started\n";
        output << "\n";
        output << "not a record\n";
        for (int i = 0; i < 100; ++i) {
            output << "2026-10-27T10:15:30|WARN|worker|job " << i << "\n";
        }
        output << "2026-10-27T10:15:30|ERROR|storage|failed\n";
        output << "2026-10-27T10:15:30|INFO|network,client|connected\n";
    }
    const concurx::StatisticsSnapshot single = run_processor(1, path);
    const concurx::StatisticsSnapshot multiple = run_processor(4, path);
    check(single.total_input_records == 105, "total lines include blank and malformed input");
    check(single.blank_lines == 1 && single.malformed_records == 1,
          "blank and malformed records are counted separately");
    check(single.processed_records == 103, "all valid records are processed exactly once");
    check(single.processed_records == multiple.processed_records, "worker counts agree");
    check(single.checksum == multiple.checksum, "checksum is deterministic across worker counts");
    check(single.severity_counts == multiple.severity_counts, "severity aggregates agree");
    check(single.component_counts == multiple.component_counts, "component aggregates agree");
    check(single.latencies_nanoseconds.size() == single.processed_records,
          "one queue-to-completion latency is recorded per processed record");
    check(concurx::percentile_nearest_rank(single.latencies_nanoseconds, 99) > 0,
          "latency percentile is calculated");
    const std::string summary_path = "concurx_test_summary.csv";
    concurx::write_summary_csv(single, summary_path);
    {
        std::ifstream summary(summary_path.c_str());
        std::string line;
        bool escaped_component_found = false;
        while (std::getline(summary, line)) {
            if (line == "\"component_network,client\",1") escaped_component_found = true;
        }
        check(escaped_component_found, "CSV escapes component names containing commas");
    }
    std::remove(summary_path.c_str());
    std::remove(path.c_str());

    {
        std::ofstream output(path.c_str());
    }
    const concurx::StatisticsSnapshot empty = run_processor(2, path);
    check(empty.processed_records == 0 && empty.checksum == 0,
          "empty file shuts down workers cleanly");
    std::remove(path.c_str());
}

}  // namespace

int main() {
    test_parser();
    test_queue();
    test_processor();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All ConcurX tests passed\n";
    return 0;
}
