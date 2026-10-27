#include "LogParser.hpp"

#include <cctype>
#include <sstream>

namespace concurx {
namespace {

bool valid_timestamp(const std::string& value) {
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':' || value[16] != ':') {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 &&
            !std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    const int month = (value[5] - '0') * 10 + value[6] - '0';
    const int day = (value[8] - '0') * 10 + value[9] - '0';
    const int hour = (value[11] - '0') * 10 + value[12] - '0';
    const int minute = (value[14] - '0') * 10 + value[15] - '0';
    const int second = (value[17] - '0') * 10 + value[18] - '0';
    if (month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                     (value[2] - '0') * 10 + value[3] - '0';
    const bool leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    static const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int maximum_day = month == 2 && leap_year ? 29 : days_per_month[month - 1];
    return day >= 1 && day <= maximum_day;
}

bool blank_line(const std::string& line) {
    for (std::string::const_iterator it = line.begin(); it != line.end(); ++it) {
        if (!std::isspace(static_cast<unsigned char>(*it))) {
            return false;
        }
    }
    return true;
}

}  // namespace

const char* severity_name(Severity severity) {
    switch (severity) {
        case Severity::Debug: return "DEBUG";
        case Severity::Info: return "INFO";
        case Severity::Warn: return "WARN";
        case Severity::Error: return "ERROR";
        case Severity::Count: break;
    }
    return "UNKNOWN";
}

bool parse_severity(const std::string& value, Severity& severity) {
    if (value == "DEBUG") severity = Severity::Debug;
    else if (value == "INFO") severity = Severity::Info;
    else if (value == "WARN") severity = Severity::Warn;
    else if (value == "ERROR") severity = Severity::Error;
    else return false;
    return true;
}

ParseResult LogParser::parse(const std::string& line) const {
    ParseResult result = {false, false, LogRecord(), std::string()};
    if (blank_line(line)) {
        result.blank = true;
        return result;
    }

    const std::size_t first = line.find('|');
    const std::size_t second = first == std::string::npos ? first : line.find('|', first + 1);
    const std::size_t third = second == std::string::npos ? second : line.find('|', second + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos) {
        result.error = "expected timestamp|level|component|message with non-empty fields";
        return result;
    }

    const std::string timestamp = line.substr(0, first);
    const std::string level = line.substr(first + 1, second - first - 1);
    const std::string component = line.substr(second + 1, third - second - 1);
    const std::string message = line.substr(third + 1);
    if (!valid_timestamp(timestamp)) {
        result.error = "timestamp must use YYYY-MM-DDTHH:MM:SS";
        return result;
    }
    if (component.empty() || message.empty()) {
        result.error = "component and message must not be empty";
        return result;
    }

    Severity severity;
    if (!parse_severity(level, severity)) {
        result.error = "unsupported severity level";
        return result;
    }

    result.success = true;
    result.record.timestamp = timestamp;
    result.record.severity = severity;
    result.record.component = component;
    result.record.message = message;
    return result;
}

}  // namespace concurx
