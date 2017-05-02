/* 
 * File:   collect_mem_windows.cpp
 * Author: alex
 *
 * Created on May 2, 2017, 10:44 PM
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

#include "staticlib/json.hpp"
#include "staticlib/utils.hpp"

#include "memlog_exception.hpp"

namespace memlog {

sl::json::value collect_mem_from_os() {
    PROCESS_MEMORY_COUNTERS pmc;
    ::memset(std::addressof(pmc), '\0', sizeof(pmc));
    auto err = ::GetProcessMemoryInfo(GetCurrentProcess(), std::addressof(pmc), sizeof(pmc));
    if (0 == err) {
        throw memlog_exception(TRACEMSG("'GetProcessMemoryInfo' error: [" +
                sl::utils::errcode_to_string(GetLastError()) + "]"));
    }
    return {
        { "overall", pmc.WorkingSetSize },
        { "PageFaultCount", static_cast<uint64_t>(pmc.PageFaultCount) },
        { "PeakWorkingSetSize", pmc.PeakWorkingSetSize },
        { "WorkingSetSize", pmc.WorkingSetSize },
        { "QuotaPeakPagedPoolUsage", pmc.QuotaPeakPagedPoolUsage },
        { "QuotaPagedPoolUsage", pmc.QuotaPagedPoolUsage },
        { "QuotaPeakNonPagedPoolUsage", pmc.QuotaPeakNonPagedPoolUsage },
        { "QuotaNonPagedPoolUsage", pmc.QuotaNonPagedPoolUsage },
        { "PagefileUsage", pmc.PagefileUsage },
        { "PeakPagefileUsage", pmc.PeakPagefileUsage }
    };
}

} // namespace