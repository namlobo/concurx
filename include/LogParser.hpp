#pragma once

#include "LogRecord.hpp"

#include <string>

namespace concurx {

struct ParseResult {
    bool success;
    bool blank;
    LogRecord record;
    std::string error;
};

class LogParser {
public:
    ParseResult parse(const std::string& line) const;
};

}  // namespace concurx
