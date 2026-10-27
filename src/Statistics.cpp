#include "Statistics.hpp"

#include <algorithm>
#include <stdexcept>

namespace concurx {

Statistics::Statistics() {
    data_.total_input_records = 0;
    data_.processed_records = 0;
    data_.malformed_records = 0;
    data_.blank_lines = 0;
    data_.severity_counts.fill(0);
    data_.checksum = 0;
    data_.elapsed_nanoseconds = 0;
}

void Statistics::record_input() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.total_input_records;
}

void Statistics::record_blank() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.blank_lines;
}

void Statistics::record_malformed() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.malformed_records;
}

void Statistics::record_processed(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.processed_records;
    ++data_.severity_counts[static_cast<std::size_t>(record.severity)];
    ++data_.component_counts[record.component];
    data_.checksum += record_hash(record);
    const std::chrono::steady_clock::time_point finished = std::chrono::steady_clock::now();
    const std::uint64_t latency_nanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            finished - record.enqueued_at).count());
    data_.latencies_nanoseconds.push_back(latency_nanoseconds);
}

StatisticsSnapshot Statistics::snapshot(std::uint64_t elapsed_nanoseconds) const {
    std::lock_guard<std::mutex> lock(mutex_);
    StatisticsSnapshot result = data_;
    result.elapsed_nanoseconds = elapsed_nanoseconds;
    return result;
}

std::uint64_t record_hash(const LogRecord& record) {
    const std::string value = record.timestamp + "|" + severity_name(record.severity) +
                              "|" + record.component + "|" + record.message;
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        hash ^= static_cast<unsigned char>(*it);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t percentile_nearest_rank(std::vector<std::uint64_t> values, unsigned percentile) {
    if (percentile == 0 || percentile > 100) {
        throw std::invalid_argument("percentile must be between 1 and 100");
    }
    if (values.empty()) {
        return 0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t rank = (percentile * values.size() + 99) / 100;
    return values[rank - 1];
}

}  // namespace concurx
