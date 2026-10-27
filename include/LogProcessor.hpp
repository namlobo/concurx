#pragma once

#include "Statistics.hpp"

#include <cstddef>
#include <string>

namespace concurx {

struct ProcessorOptions {
    std::size_t worker_count;
    std::size_t queue_capacity;
};

class LogProcessor {
public:
    explicit LogProcessor(ProcessorOptions options);
    StatisticsSnapshot process_file(const std::string& input_path) const;

private:
    ProcessorOptions options_;
};

void write_summary_csv(const StatisticsSnapshot& statistics, const std::string& output_path);
void write_summary_csv(const StatisticsSnapshot& statistics);

}  // namespace concurx
