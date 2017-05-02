/* 
 * File:   collect_mem_linux.cpp
 * Author: alex
 *
 * Created on May 2, 2017, 10:44 PM
 */

#include "collect_mem_os.hpp"

#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

namespace memlog {

namespace { // anonymous

uint64_t parse_value(const std::string& line, const std::string& prefix) {
    // std::regex is broken in gcc < 4.9, so parsing manually
    size_t idx = prefix.length() + 1;
    while (idx < line.length() && !::isdigit(line[idx])) {
        idx += 1;
    }
    size_t start = idx;
    while (idx < line.length() && ::isdigit(line[idx])) {
        idx += 1;
    }
    std::string num = line.substr(start, idx - start);
    return sl::utils::parse_uint64(num) * 1024;
}

} // namespace

sl::json::value collect_mem_from_os() {
    // man7.org/linux/man-pages/man5/proc.5.html
    auto src = sl::io::make_buffered_source(sl::tinydir::file_source("/proc/self/status"));    
    uint64_t peak = 0;
    uint64_t size = 0;
    uint64_t lck = 0;
    uint64_t pin = 0;
    uint64_t hwm = 0;
    uint64_t rss = 0;
    uint64_t data = 0;
    uint64_t stk = 0;
    uint64_t exe = 0;
    uint64_t lib = 0;
    uint64_t pte = 0;
    uint64_t swap = 0;
    std::string line;    
    while((line = src.read_line()).length() > 0) {
        if (sl::utils::starts_with(line, "VmPeak")) {
            peak = parse_value(line, "VmPeak");
        } else if (sl::utils::starts_with(line, "VmSize")) {
            size = parse_value(line, "VmSize");
        } else if (sl::utils::starts_with(line, "VmLck")) {
            lck = parse_value(line, "VmLck");
        } else if (sl::utils::starts_with(line, "VmPin")) {
            pin = parse_value(line, "VmPin");
        } else if (sl::utils::starts_with(line, "VmHWM")) {
            hwm = parse_value(line, "VmHWM");
        } else if (sl::utils::starts_with(line, "VmRSS")) {
            rss = parse_value(line, "VmRSS");
        } else if (sl::utils::starts_with(line, "VmData")) {
            data = parse_value(line, "VmData");
        } else if (sl::utils::starts_with(line, "VmStk")) {
            stk = parse_value(line, "VmStk");
        } else if (sl::utils::starts_with(line, "VmExe")) {
            exe = parse_value(line, "VmExe");
        } else if (sl::utils::starts_with(line, "VmLib")) {
            lib = parse_value(line, "VmLib");
        } else if (sl::utils::starts_with(line, "VmPTE")) {
            pte = parse_value(line, "VmPTE");
        } else if (sl::utils::starts_with(line, "VmSwap")) {
            swap = parse_value(line, "VmSwap");
        }
    }
    return {
        {"overall", rss},
        {"VmPeak", peak},
        {"VmSize", size},
        {"VmLck", lck},
        {"VmPin", pin},
        {"VmHWM", hwm},
        {"VmRSS", rss},
        {"VmData", data},
        {"VmStk", stk},
        {"VmExe", exe},
        {"VmLib", lib},
        {"VmPTE", pte},
        {"VmSwap", swap},
    };
}

} // namespace
