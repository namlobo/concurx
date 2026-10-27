#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace concurx {

enum class Severity : std::size_t {
    Debug = 0,
    Info,
    Warn,
    Error,
    Count
};

struct LogRecord {
    std::string timestamp;
    Severity severity;
    std::string component;
    std::string message;
    std::chrono::steady_clock::time_point enqueued_at;
};

const char* severity_name(Severity severity);
bool parse_severity(const std::string& value, Severity& severity);

}  // namespace concurx
