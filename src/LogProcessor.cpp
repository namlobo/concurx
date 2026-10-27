#include "LogProcessor.hpp"

#include "BoundedQueue.hpp"
#include "LogParser.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace concurx {
namespace {

std::string csv_escape(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        if (*it == '"') escaped += "\"\"";
        else escaped += *it;
    }
    escaped += "\"";
    return escaped;
}

void write_csv(std::ostream& output, const StatisticsSnapshot& statistics) {
    const double seconds = static_cast<double>(statistics.elapsed_nanoseconds) / 1000000000.0;
    const double throughput = seconds > 0.0
        ? static_cast<double>(statistics.processed_records) / seconds : 0.0;
    output << "metric,value\n";
    output << "total_input_records," << statistics.total_input_records << "\n";
    output << "processed_records," << statistics.processed_records << "\n";
    output << "malformed_records," << statistics.malformed_records << "\n";
    output << "blank_lines," << statistics.blank_lines << "\n";
    output << "elapsed_seconds," << std::setprecision(9) << seconds << "\n";
    output << "throughput_records_per_second," << std::setprecision(9) << throughput << "\n";
    output << "p50_latency_nanoseconds,"
           << percentile_nearest_rank(statistics.latencies_nanoseconds, 50) << "\n";
    output << "p95_latency_nanoseconds,"
           << percentile_nearest_rank(statistics.latencies_nanoseconds, 95) << "\n";
    output << "p99_latency_nanoseconds,"
           << percentile_nearest_rank(statistics.latencies_nanoseconds, 99) << "\n";
    output << "checksum," << statistics.checksum << "\n";
    for (std::size_t i = 0; i < statistics.severity_counts.size(); ++i) {
        output << "severity_" << severity_name(static_cast<Severity>(i)) << ","
               << statistics.severity_counts[i] << "\n";
    }
    for (std::map<std::string, std::uint64_t>::const_iterator it =
             statistics.component_counts.begin();
         it != statistics.component_counts.end(); ++it) {
        output << csv_escape("component_" + it->first) << "," << it->second << "\n";
    }
}

}  // namespace

LogProcessor::LogProcessor(ProcessorOptions options) : options_(options) {
    if (options_.worker_count == 0) {
        throw std::invalid_argument("worker count must be greater than zero");
    }
    if (options_.queue_capacity == 0) {
        throw std::invalid_argument("queue capacity must be greater than zero");
    }
}

StatisticsSnapshot LogProcessor::process_file(const std::string& input_path) const {
    std::ifstream input(input_path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open input file: " + input_path);
    }

    BoundedQueue<LogRecord> queue(options_.queue_capacity);
    Statistics statistics;
    LogParser parser;
    std::mutex error_mutex;
    std::exception_ptr worker_error;
    std::vector<std::thread> workers;
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();

    try {
        workers.reserve(options_.worker_count);
        for (std::size_t i = 0; i < options_.worker_count; ++i) {
            workers.push_back(std::thread([&queue, &statistics, &error_mutex, &worker_error] {
                try {
                    LogRecord record;
                    while (queue.pop(record)) {
                        statistics.record_processed(record);
                    }
                } catch (...) {
                    {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!worker_error) worker_error = std::current_exception();
                    }
                    queue.close();
                }
            }));
        }
    } catch (...) {
        queue.close();
        for (std::size_t i = 0; i < workers.size(); ++i) {
            if (workers[i].joinable()) workers[i].join();
        }
        throw;
    }

    std::exception_ptr producer_error;
    try {
        std::string line;
        while (std::getline(input, line)) {
            statistics.record_input();
            ParseResult parsed = parser.parse(line);
            if (parsed.blank) {
                statistics.record_blank();
            } else if (!parsed.success) {
                statistics.record_malformed();
            } else {
                parsed.record.enqueued_at = std::chrono::steady_clock::now();
                if (!queue.push(std::move(parsed.record))) {
                    break;
                }
            }
        }
        if (input.bad()) {
            throw std::runtime_error("error while reading input file: " + input_path);
        }
    } catch (...) {
        producer_error = std::current_exception();
    }

    queue.close();
    for (std::size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].joinable()) workers[i].join();
    }

    if (producer_error) std::rethrow_exception(producer_error);
    if (worker_error) std::rethrow_exception(worker_error);

    const std::chrono::steady_clock::time_point finished = std::chrono::steady_clock::now();
    const std::uint64_t elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count());
    return statistics.snapshot(elapsed);
}

void write_summary_csv(const StatisticsSnapshot& statistics, const std::string& output_path) {
    std::ofstream output(output_path.c_str());
    if (!output) {
        throw std::runtime_error("cannot open summary output file: " + output_path);
    }
    write_csv(output, statistics);
    if (!output) {
        throw std::runtime_error("failed while writing summary output file: " + output_path);
    }
}

void write_summary_csv(const StatisticsSnapshot& statistics) {
    write_csv(std::cout, statistics);
    if (!std::cout) {
        throw std::runtime_error("failed while writing summary to standard output");
    }
}

}  // namespace concurx
