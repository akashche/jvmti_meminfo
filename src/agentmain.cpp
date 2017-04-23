/* 
 * File:   agentmain.cpp
 * Author: alex
 *
 * Created on April 12, 2017, 4:29 PM
 */

#include <cctype>
#include <iostream>
#include <string>

#include "jmm.h"

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/jni.hpp"
#include "staticlib/jvmti.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

class meminfo : public sl::jvmti::agent_base<meminfo> {
public:
    meminfo(JavaVM* jvm, char* options) :
    sl::jvmti::agent_base<meminfo>(jvm, options) { }
    
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
        uint32_t os = collect_mem_from_os();
        uint32_t jvm = collect_mem_from_jvm();
        std::cout << os << " : " << jvm << std::endl;
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
//        // auto scoped_jni = sl::jni::thread_local_jni_env_ptr();
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
        // auto scoped_jni = sl::jni::thread_local_jni_env_ptr();
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
};

meminfo* global_mi = nullptr;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* /* reserved */) {
    try {
        global_mi = new meminfo(jvm, options);
        return JNI_OK;
    } catch (const std::exception& e) {
        std::cout << TRACEMSG(e.what() + "\nInitialization error") << std::endl;
        return JNI_ERR;
    }
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* /* vm */) {
    delete global_mi;
    std::cout << "Shutdown complete" << std::endl;
}
