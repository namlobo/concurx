#include "LogProcessor.hpp"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void print_help() {
    std::cout
        << "ConcurX - concurrent log processing engine\n"
        << "Usage:\n"
        << "  concurx --input <path> [--workers N] [--queue-capacity N] [--output <path>]\n"
        << "  concurx --generate N [--input <path>] [--malformed-every N]\n"
        << "          [--workers N] [--queue-capacity N] [--output <path>]\n"
        << "  concurx --help\n\n"
        << "If --generate is used without --input, records are written to generated.log.\n"
        << "Summary CSV is printed to stdout and optionally written to --output.\n";
}

std::size_t parse_size(const std::string& value, const std::string& option) {
    if (value.empty() || value[0] == '-') {
        throw std::invalid_argument("invalid value for " + option + ": " + value);
    }
    errno = 0;
    char* end = NULL;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0' ||
        parsed > static_cast<unsigned long long>(static_cast<std::size_t>(-1))) {
        throw std::invalid_argument("invalid value for " + option + ": " + value);
    }
    return static_cast<std::size_t>(parsed);
}

void generate_input(const std::string& path, std::size_t count, std::size_t malformed_every) {
    static const char* levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    static const char* components[] = {"scheduler", "storage", "network", "worker"};
    std::ofstream output(path.c_str());
    if (!output) throw std::runtime_error("cannot open generated input file: " + path);
    for (std::size_t i = 0; i < count; ++i) {
        if (malformed_every != 0 && (i + 1) % malformed_every == 0) {
            output << "malformed-record-" << i << "\n";
        } else {
            output << "2026-10-27T10:15:30|" << levels[i % 4] << "|"
                   << components[i % 4] << "|synthetic event " << i;
            if (i % 7 == 0) output << "|extra-delimiter";
            output << "\n";
        }
    }
    if (!output) throw std::runtime_error("failed while writing generated input file: " + path);
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::string input_path;
        std::string output_path;
        std::size_t workers = std::thread::hardware_concurrency();
        std::size_t capacity = 256;
        std::size_t generated_count = 0;
        std::size_t malformed_every = 0;
        bool generate = false;
        bool malformed_option_supplied = false;

        if (workers == 0) workers = 1;
        for (int i = 1; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "--help") {
                print_help();
                return 0;
            } else if (option == "--input" || option == "--workers" ||
                       option == "--queue-capacity" || option == "--output" ||
                       option == "--generate" || option == "--malformed-every") {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + option);
                const std::string value(argv[++i]);
                if (option == "--input") input_path = value;
                else if (option == "--output") output_path = value;
                else if (option == "--workers") workers = parse_size(value, option);
                else if (option == "--queue-capacity") capacity = parse_size(value, option);
                else if (option == "--generate") {
                    generated_count = parse_size(value, option);
                    generate = true;
                } else {
                    malformed_every = parse_size(value, option);
                    malformed_option_supplied = true;
                }
            } else {
                throw std::invalid_argument("unknown option: " + option);
            }
        }

        if (workers == 0) throw std::invalid_argument("--workers must be greater than zero");
        if (capacity == 0) throw std::invalid_argument("--queue-capacity must be greater than zero");
        if (generate) {
            if (input_path.empty()) input_path = "generated.log";
            generate_input(input_path, generated_count, malformed_every);
        } else if (input_path.empty()) {
            throw std::invalid_argument("provide --input or --generate");
        } else if (malformed_option_supplied) {
            throw std::invalid_argument("--malformed-every requires --generate");
        }

        concurx::LogProcessor processor = concurx::LogProcessor({workers, capacity});
        const concurx::StatisticsSnapshot statistics = processor.process_file(input_path);
        concurx::write_summary_csv(statistics);
        if (!output_path.empty()) concurx::write_summary_csv(statistics, output_path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "concurx: " << error.what() << "\n";
        return 2;
    }
}
