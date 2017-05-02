/* 
 * File:   agentmain.cpp
 * Author: alex
 *
 * Created on April 12, 2017, 4:29 PM
 */

#include <cctype>
#include <cstring>
#include <iostream>
#include <string>

#include "staticlib/config/os.hpp"
#ifdef STATICLIB_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif // STATICLIB_WINDOWS

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/jni.hpp"
#include "staticlib/json.hpp"
#include "staticlib/jvmti.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "config.hpp"
#include "memlog_exception.hpp"

namespace memlog {

class agent : public sl::jvmti::agent_base<agent> {
    config cf;
    sl::json::array_writer<sl::io::buffered_sink<sl::tinydir::file_sink>> log_writer;
    
public:
    agent(JavaVM* jvm, char* options) :
    sl::jvmti::agent_base<agent>(jvm, options),
    cf(read_config()),
    log_writer(sl::io::make_buffered_sink(sl::tinydir::file_sink(cf.output_path_json))) {
        write_to_stdout("agent created");
    }
    
    void operator()() STATICLIB_NOEXCEPT {
        write_to_stdout("agent initialized");
        try {
            while(sl::jni::static_java_vm().running()) {
                collect_and_write_measurement();
                sl::jni::static_java_vm().thread_sleep_before_shutdown(std::chrono::milliseconds(1000));
            }
            // all spawned threads must be joined at this point
            write_to_stdout("shutting down");
        } catch(const std::exception& e) {
            std::cerr << TRACEMSG(e.what() + "\nWorker error") << std::endl;
        } catch(...) {
            std::cerr << TRACEMSG("Unexpected worker error") << std::endl;
        }
    }
    
    bool can_write_stdout() {
        return cf.stdout_messages;
    }
    
private:
    void collect_and_write_measurement() {
        uint64_t os = collect_mem_from_os();
        uint64_t jvm = collect_mem_from_jvm();
        log_writer.write({
            { "os", os / (1 << 20)},
            { "jvm", jvm / (1 << 20)}
        });
    }
    
    uint64_t collect_mem_from_os() {
#if defined(STATICLIB_LINUX)
        return collect_mem_linux();
#elif defined(STATICLIB_WINDOWS)
        return collect_mem_windows();
#else  
        static_assert(false, "Unsupported OS");
#endif // STATICLIB_LINUX
    }
    
    uint64_t collect_mem_from_jvm() {
        return collect_mem_jmm();
    }
    
//    uint64_t collect_mem_jmx() {
//        auto mfcls = sl::jni::jclass_ptr("java/lang/management/ManagementFactory");
//        auto membeancls = sl::jni::jclass_ptr("java/lang/management/MemoryMXBean");
//        auto membean = mfcls.call_static_object_method(membeancls, "getMemoryMXBean",
//                "()Ljava/lang/management/MemoryMXBean;");
//        auto mucls = sl::jni::jclass_ptr("java/lang/management/MemoryUsage");
//        auto muheap = membean.call_object_method(mucls, "getHeapMemoryUsage",
//                "()Ljava/lang/management/MemoryUsage;");
//        auto resheap = muheap.call_method<jlong>("getCommitted", "()J", &JNIEnv::CallLongMethod);
//        auto munh = membean.call_object_method(mucls, "getNonHeapMemoryUsage",
//                "()Ljava/lang/management/MemoryUsage;");
//        auto resnh = munh.call_method<jlong>("getCommitted", "()J", &JNIEnv::CallLongMethod);
//        return static_cast<uint64_t> (resheap) + static_cast<uint64_t> (resnh);
//    }
    
    uint64_t collect_mem_jmm() {
        auto mucls = sl::jni::jclass_ptr("java/lang/management/MemoryUsage");
        auto muheap = jmm.call_object_method(mucls, &JmmInterface::GetMemoryUsage, true);
        auto resheap = muheap.call_method<jlong>("getCommitted", "()J", &JNIEnv::CallLongMethod);
        auto munh = jmm.call_object_method(mucls, &JmmInterface::GetMemoryUsage, false);
        auto resnh = munh.call_method<jlong>("getCommitted", "()J", &JNIEnv::CallLongMethod);
        return static_cast<uint64_t> (resheap) + static_cast<uint64_t> (resnh);
    }
    
#ifdef STATICLIB_LINUX
    uint64_t collect_mem_linux() {
        static std::string prefix = "VmRSS:";
        auto src = sl::io::make_buffered_source(sl::tinydir::file_source("/proc/self/status"));
        std::string line;
        // std::regex is broken in gcc < 4.9, so parsing manually
        do {
            line = src.read_line();
            if (0 == line.length()) {
                return 0;
            }
        } while (!sl::utils::starts_with(line, prefix));
        size_t idx = prefix.length();
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
#endif // STATICLIB_LINUX

#ifdef STATICLIB_WINDOWS
    uint64_t collect_mem_windows() {
        PROCESS_MEMORY_COUNTERS pmc;
        ::memset(std::addressof(pmc), '\0', sizeof(pmc));
        auto err = ::GetProcessMemoryInfo(GetCurrentProcess(), std::addressof(pmc), sizeof(pmc));
        if (0 == err) {
            throw memlog_exception(TRACEMSG("'GetProcessMemoryInfo' error: [" + 
                    sl::utils::errcode_to_string(GetLastError()) + "]"));
        }
        return static_cast<uint64_t>(pmc.WorkingSetSize);
    }
#endif // STATICLIB_WINDOWS
    
    config read_config() {
        std::string path = [this] {
            if (!options.empty()) {
                return options;
            }
            // points to jvm/bin
            // auto exepath = sl::utils::current_executable_path();
            // auto exedir = sl::utils::strip_filename(exepath);
            // return exedir + "config.json";
            return std::string("config.json");
        }();
        auto src = sl::tinydir::file_source(path);
        auto json = sl::json::load(src);
        return config(json);
    }
    
    void write_to_stdout(const std::string& message) {
        if (cf.stdout_messages) {
            std::cout<< "memlog_agent: " << message << std::endl;
        }
    }
};

} // namespace

memlog::agent* global_agent = nullptr;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* /* reserved */) {
    try {
        global_agent = new memlog::agent(jvm, options);
        return JNI_OK;
    } catch (const std::exception& e) {
        std::cerr << TRACEMSG(e.what() + "\nInitialization error") << std::endl;
        return JNI_ERR;
    }
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* /* vm */) {
    bool can_write = global_agent->can_write_stdout();
    delete global_agent;
    if (can_write) {
        std::cout << "memlog_agent: shutdown complete" << std::endl;
    }
}
