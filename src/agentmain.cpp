/* 
 * File:   agentmain.cpp
 * Author: alex
 *
 * Created on April 12, 2017, 4:29 PM
 */

#include <jni.h>
#include <jvmti.h>

#include <cstring>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "staticlib/config.hpp"
#include "staticlib/concurrent.hpp"
#include "staticlib/io.hpp"
#include "staticlib/jni.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"

#include "jni_exception.hpp"
#include "jvmti_exception.hpp"

namespace jvmti_helper {

class jvmti_error_checker {
public:
    void operator=(jvmtiError err) {
        if (JVMTI_ERROR_NONE != err) {
            throw jvmti_exception(TRACEMSG("JVMTI error code: [" + sl::support::to_string(err) + "]"));
        }
    }
};

class jvmti_env_ptr {
    std::unique_ptr<jvmtiEnv, std::function<void(jvmtiEnv*)>> jvmti;
    
public:
    jvmti_env_ptr(JavaVM* jvm) :
    jvmti([jvm] {
        sl::jni::error_checker ec;
        jvmtiEnv* env;
        ec = jvm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JVMTI_VERSION);
        return std::unique_ptr<jvmtiEnv, std::function<void(jvmtiEnv*)>>(env, [jvm](jvmtiEnv * ptr) {
            // pass captured jvm pointer in case of destruction before global jvm pointer init
            auto scoped_jni = sl::jni::thread_local_jni_env_ptr(jvm);
            // lets crash early on error
            jvmti_error_checker ec;
            ec = ptr->DisposeEnvironment();
        });
    }()) { }

    jvmti_env_ptr (const jvmti_env_ptr&) = delete;
    
    jvmti_env_ptr& operator=(const jvmti_env_ptr&) = delete;
    
    jvmti_env_ptr(jvmti_env_ptr&& other) :
    jvmti(std::move(other.jvmti)) { }
    
    jvmti_env_ptr& operator=(jvmti_env_ptr&& other) {
        jvmti = std::move(other.jvmti);
        return *this;
    }
    
    jvmtiEnv* operator->() {
        return jvmti.get();
    }

    jvmtiEnv* get() {
        return jvmti.get();
    }
};


template<typename T> class jvmti_base {
protected:
    jvmti_env_ptr jvmti;
    std::string options;
    std::thread worker;
    
    jvmti_base(JavaVM* javavm, char* options) :
    jvmti(javavm),
    options(nullptr != options ? options : "") {        
        register_vminit_callback();
        apply_capabilities();
        // start worker
        this->worker = std::thread([this, javavm] {
            // init global jvm pointer
            sl::jni::static_java_vm(javavm);
            // wait for init
            sl::jni::static_java_vm().await_init_complete();
            // call inheritor
            static_cast<T*> (this)->operator()();
        });
    }
    
    jvmti_base(const jvmti_base&) = delete;
    
    void operator=(const jvmti_base&) = delete;
    
    // virtual is unnecessary
    ~jvmti_base() STATICLIB_NOEXCEPT {
        std::cout << "Shutting down ..." << std::endl;
        sl::jni::static_java_vm().notify_shutdown();
        worker.join();
    }
        
    std::unique_ptr<jvmtiCapabilities> capabilities() {
        auto caps = sl::support::make_unique<jvmtiCapabilities>();
        std::memset(caps.get(), 0, sizeof(*caps));
        return caps;
    }
    
private:
    void apply_capabilities() {
        auto caps = static_cast<T*> (this)->capabilities();
        jvmti_error_checker ec;
        ec = jvmti->AddCapabilities(caps.get());
    }
    
    void register_vminit_callback() {
        jvmtiEventCallbacks cbs;
        memset(std::addressof(cbs), 0, sizeof (cbs));
        cbs.VMInit = [](jvmtiEnv*, JNIEnv*, jthread) {
            sl::jni::static_java_vm().notify_init_complete();
        };
        jvmti_error_checker ec;
        ec = jvmti->SetEventCallbacks(std::addressof(cbs), sizeof (cbs));
        ec = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    }
};

} // namespace


class meminfo : public jvmti_helper::jvmti_base<meminfo> {
public:
    meminfo(JavaVM* jvm, char* options) :
    jvmti_helper::jvmti_base<meminfo>(jvm, options) { }
    
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
//        auto sink = sl::io::streambuf_sink(std::cout.rdbuf());
        auto sink = sl::io::null_sink();
        sl::io::copy_all(src, sink);
//        std::cout << std::endl << std::endl;
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
}
