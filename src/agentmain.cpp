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
                sl::jni::static_java_vm().thread_sleep_before_shutdown(std::chrono::milliseconds(5000));
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
//        std::string path = "/proc/self/statm";
//        auto src = sl::tinydir::file_source(path);
//        auto sink = sl::io::streambuf_sink(std::cout.rdbuf());
//        sl::io::copy_all(src, sink);
//        std::cout << std::endl << std::endl;
        
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
    
    uint32_t collect_mem_from_jvm() {
        // auto scoped_jni = jni_helper::thread_local_jni_ptr();
        auto rt = sl::jni::jclass_ptr("java/lang/Runtime");
        auto obj = rt.call_static_object_method("getRuntime", "()Ljava/lang/Runtime;");
        auto res = obj.call_method<jlong>("totalMemory", "()J", &JNIEnv::CallLongMethod);
        return res > 0 ? static_cast<uint64_t>(res) : 0;
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
