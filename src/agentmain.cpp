/* 
 * File:   agentmain.cpp
 * Author: alex
 *
 * Created on April 12, 2017, 4:29 PM
 */

#include <cctype>
#include <iostream>
#include <string>

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
    sl::io::buffered_sink<sl::tinydir::file_sink> json_log_file;
    
public:
    agent(JavaVM* jvm, char* options) :
    sl::jvmti::agent_base<agent>(jvm, options),
    cf(read_config()),
    json_log_file(sl::tinydir::file_sink(cf.output_path_json)) {
        json_log_file.write({"[\n"});
        auto zero_entry = sl::json::value({
            { "mem_os", 0 },
            { "mem_jvm", 0 }
        }).dumps();
        json_log_file.write({zero_entry});
    }
    
    ~agent() STATICLIB_NOEXCEPT {
        try {
            json_log_file.write({"\n]\n"});
        } catch (const std::exception& e) {
            std::cout << TRACEMSG(e.what() + "\nAgent destruction error") << std::endl;
        } catch (...) {
            std::cout << TRACEMSG("Unexpected agent destruction error") << std::endl;
        }
    }
    
    void operator()() STATICLIB_NOEXCEPT {
        try {
            while(sl::jni::static_java_vm().running()) {
                collect_and_write_measurement();
                sl::jni::static_java_vm().thread_sleep_before_shutdown(std::chrono::milliseconds(1000));
            }
            // all spawned threads must be joined at this point
        } catch(const std::exception& e) {
            std::cout << TRACEMSG(e.what() + "\nWorker error") << std::endl;
        } catch(...) {
            std::cout << TRACEMSG("Unexpected worker error") << std::endl;
        }
    }
    
private:
    void collect_and_write_measurement() {
        uint64_t os = collect_mem_from_os();
        uint64_t jvm = collect_mem_from_jvm();
        auto entry = sl::json::value({
            { "mem_os", os },
            { "mem_jvm", jvm }
        }).dumps();
        json_log_file.write({",\n"});
        json_log_file.write({entry});
    }
    
    uint64_t collect_mem_from_os() {
#if defined(STATICLIB_LINUX)
        return collect_mem_linux();
#elif defined(STATICLIB_WINDOWS)
        static_assert(false, "TODO");
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
    
    config read_config() {
        std::string path = [this] {
            if (!options.empty()) {
                return options;
            }
            auto exepath = sl::utils::current_executable_path();
            auto exedir = sl::utils::strip_filename(exepath);
            return exedir + "config.json";
        }();
        auto src = sl::tinydir::file_source(path);
        auto json = sl::json::load(src);
        return config(json);
    }
};

} // namespace

memlog::agent* global_agent = nullptr;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* /* reserved */) {
    try {
        global_agent = new memlog::agent(jvm, options);
        return JNI_OK;
    } catch (const std::exception& e) {
        std::cout << TRACEMSG(e.what() + "\nInitialization error") << std::endl;
        return JNI_ERR;
    }
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* /* vm */) {
    delete global_agent;
    std::cout << "Shutdown complete" << std::endl;
}
