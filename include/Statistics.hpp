#pragma once

#include "LogRecord.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace concurx {

struct StatisticsSnapshot {
    std::uint64_t total_input_records;
    std::uint64_t processed_records;
    std::uint64_t malformed_records;
    std::uint64_t blank_lines;
    std::array<std::uint64_t, static_cast<std::size_t>(Severity::Count)> severity_counts;
    std::map<std::string, std::uint64_t> component_counts;
    std::uint64_t checksum;
    std::uint64_t elapsed_nanoseconds;
    std::vector<std::uint64_t> latencies_nanoseconds;
};

class Statistics {
public:
    Statistics();

    void record_input();
    void record_blank();
    void record_malformed();
    void record_processed(const LogRecord& record);
    StatisticsSnapshot snapshot(std::uint64_t elapsed_nanoseconds) const;

private:
    mutable std::mutex mutex_;
    StatisticsSnapshot data_;
};

std::uint64_t percentile_nearest_rank(std::vector<std::uint64_t> values, unsigned percentile);
std::uint64_t record_hash(const LogRecord& record);

}  // namespace concurx
