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
#include <string>
#include <thread>
#include <ios>

#include "staticlib/config.hpp"
#include "staticlib/concurrent.hpp"
#include "staticlib/io.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"

#include "jvmti_exception.hpp"

#ifdef STATICLIB_LINUX
#include <unistd.h>
#endif // STATICLIB_LINUX

namespace jvmti_helper {

class jni_error_checker {
public:
    void operator=(jint err) {
        if (JNI_OK != err) {
            throw jvmti_exception(TRACEMSG("JNI error code: [" + sl::support::to_string(err) + "]"));
        }
    }
};

class jvmti_error_checker {
public:
    void operator=(jvmtiError err) {
        if (JVMTI_ERROR_NONE != err) {
            throw jvmti_exception(TRACEMSG("JVMTI error code: [" + sl::support::to_string(err) + "]"));
        }
    }
};

sl::support::observer_ptr<sl::concurrent::countdown_latch> static_vminit_latch(
        sl::concurrent::countdown_latch* latch = nullptr) {
    static sl::support::observer_ptr<sl::concurrent::countdown_latch> vminit_latch = 
            sl::support::make_observer_ptr(latch);
    return vminit_latch;
}

template<typename T> 
class jvmti_ctx {
protected:    
    sl::support::observer_ptr<JavaVM> javavm;
    std::string options;
    sl::support::observer_ptr<jvmtiEnv> jvmti;
    sl::concurrent::countdown_latch vminit_latch;
    std::atomic<bool> shutdown_flag;
    sl::concurrent::condition_latch waiting_latch;
    std::thread worker;
    jvmti_error_checker ec;
    
    jvmti_ctx(JavaVM* javavm, char* options) :
    javavm(javavm),
    options(nullptr != options ? options : ""),
    jvmti([javavm] {
        jni_error_checker ec;
        jvmtiEnv* env;
        ec = javavm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JVMTI_VERSION);
        return env;
    }()),
    vminit_latch(1),
    shutdown_flag(false),
    waiting_latch([this] {
        return this->shutdown_flag.load(std::memory_order_acquire);
    }) {
        // register init callback, latch is passed through global var
        jvmtiEventCallbacks cbs;
        memset(std::addressof(cbs), 0, sizeof (cbs));
        static_vminit_latch(std::addressof(vminit_latch));
        cbs.VMInit = [](jvmtiEnv*, JNIEnv*, jthread) {
            static_vminit_latch()->count_down();
        };
        ec = jvmti->SetEventCallbacks(std::addressof(cbs), sizeof (cbs));
        ec = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
        
        // start worker
        this->worker = std::thread([this] {
            // wait for init
            this->vminit_latch.await();
            // call inheritor
            static_cast<T*> (this)->operator()();
        });
    }
    
    jvmti_ctx(const jvmti_ctx&) = delete;
    
    void operator=(const jvmti_ctx&) = delete;
    
    // virtual is unnecessary
    ~jvmti_ctx() STATICLIB_NOEXCEPT {
        std::cout << "Shutting down ..." << std::endl;
        shutdown_flag.store(true, std::memory_order_release);
        waiting_latch.notify_all();
        worker.join();
    }
    
    sl::support::observer_ptr<JNIEnv> jni_env() {
        JNIEnv* env;
        auto getenv_err = javavm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JNI_VERSION_1_6);
        switch (getenv_err) {
        case JNI_OK:
            return sl::support::make_observer_ptr(env);
        case JNI_EDETACHED:
            auto attach_err = javavm->AttachCurrentThread(reinterpret_cast<void**> (std::addressof(env)), nullptr);
            if (JNI_OK == attach_err) {
                return sl::support::make_observer_ptr(env);
            } else {
                throw jvmti_exception(TRACEMSG("JNI 'AttachCurrentThread' error code: [" + sl::support::to_string(attach_err) + "]"));
            }
        default:
            throw jvmti_exception(TRACEMSG("JNI 'GetEnv' error code: [" + sl::support::to_string(getenv_err) + "]"));
        }
    }
};

} // namespace


class meminfo : public jvmti_helper::jvmti_ctx<meminfo> {
public:
    meminfo(JavaVM* jvm, char* options) :
    jvmti_helper::jvmti_ctx<meminfo>(jvm, options) { }
    
    void operator()() STATICLIB_NOEXCEPT {
        try {
            while(!shutdown_flag.load(std::memory_order_acquire)) {
                print_mem();
                waiting_latch.await(std::chrono::milliseconds(5000));
            }
        } catch(const std::exception& e) {
            std::cout << TRACEMSG(e.what() + "\nWorker error") << std::endl;
        } catch(...) {
            std::cout << TRACEMSG("Unexpected worker error") << std::endl;
        }
    }
    
private:
    void print_mem() {
//        auto pid = ::getpid();
        std::string path = "/proc/self/status";
        auto src = sl::tinydir::file_source(path);
        auto sink = sl::io::streambuf_sink(std::cout.rdbuf());
        sl::io::copy_all(src, sink);
        std::cout << std::endl << std::endl;
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
