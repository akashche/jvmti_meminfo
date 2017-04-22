/* 
 * File:   agentmain.cpp
 * Author: alex
 *
 * Created on April 12, 2017, 4:29 PM
 */

#include <iostream>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/jni.hpp"
#include "staticlib/jvmti.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"

class meminfo : public sl::jvmti::agent_base<meminfo> {
public:
    meminfo(JavaVM* jvm, char* options) :
    sl::jvmti::agent_base<meminfo>(jvm, options) { }
    
    void operator()() STATICLIB_NOEXCEPT {
        try {
            while(sl::jni::static_java_vm().running()) {
                print_mem();
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
    void print_mem() {
//        auto scoped_jni = jni_helper::thread_local_jni_ptr();
        auto rt = sl::jni::jclass_ptr("java/lang/Runtime");
        auto obj = rt.call_static_object_method("getRuntime", "()Ljava/lang/Runtime;");
        auto res = obj.call_method<jlong>("totalMemory", "()J", &JNIEnv::CallLongMethod);
        std::cout << res << std::endl;
        std::string path = "/proc/self/status";
        auto src = sl::tinydir::file_source(path);
        auto sink = sl::io::streambuf_sink(std::cout.rdbuf());
        sl::io::copy_all(src, sink);
        std::cout << std::endl << std::endl;
        std::cout << res << std::endl;
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
