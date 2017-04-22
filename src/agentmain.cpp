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
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"

#include "jni_exception.hpp"
#include "jvmti_exception.hpp"

namespace jni_helper {

class java_vm_ptr;

java_vm_ptr& static_java_vm(JavaVM* jvm = nullptr);

class jni_error_checker {
public:
    void operator=(jint err) {
        if (JNI_OK != err) {
            throw jni_exception(TRACEMSG("JNI error code: [" + sl::support::to_string(err) + "]"));
        }
    }
};

class java_vm_ptr {
    sl::support::observer_ptr<JavaVM> jvm;
    sl::concurrent::countdown_latch init_latch;
    std::atomic<bool> shutdown_flag;
    sl::concurrent::condition_latch shutdown_latch;

public:
    java_vm_ptr(JavaVM* vm) :
    jvm(vm),
    init_latch(1),
    shutdown_flag(false),
    shutdown_latch([this] {

        return !this->running();
    }) { }

    java_vm_ptr(const java_vm_ptr&) = delete;

    java_vm_ptr& operator=(const java_vm_ptr&) = delete;

    JavaVM* operator->() {
        return jvm.get();
    }

    JavaVM* get() {
        return jvm.get();
    }

    bool running() {
        return !shutdown_flag.load(std::memory_order_acquire);
    }

    void await_init_complete() {
        init_latch.await();
    }

    void notify_init_complete() {
        init_latch.count_down();
    }

    void thread_sleep_before_shutdown(std::chrono::milliseconds millis) {
        shutdown_latch.await(millis);
    }

    void notify_shutdown() {
        shutdown_flag.store(true, std::memory_order_release);
        shutdown_latch.notify_all();
    }
};

class thread_local_jni_ptr {
    std::unique_ptr<JNIEnv, std::function<void(JNIEnv*)>> jni;
    
public:
    thread_local_jni_ptr() :
    jni([] {
        auto& jvm = static_java_vm();
        JNIEnv* env;
        auto getenv_err = jvm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JNI_VERSION_1_6);
        switch (getenv_err) {
        case JNI_OK:
            return std::unique_ptr<JNIEnv, std::function<void(JNIEnv*)>>(env, [](JNIEnv*) { /* no-op */ });
        case JNI_EDETACHED: {
            auto attach_err = jvm->AttachCurrentThread(reinterpret_cast<void**> (std::addressof(env)), nullptr);
            if (JNI_OK == attach_err) {
                return std::unique_ptr<JNIEnv, std::function<void(JNIEnv*)>>(env, [](JNIEnv*) {
                    // TODO: JNIEnv lifecycle
//                    auto detach_err = static_java_vm()->DetachCurrentThread();
//                    if (JNI_OK != detach_err) {
//                        // something went wrong, lets crash early
//                        throw jni_exception(TRACEMSG("JNI 'DetachCurrentThread' error code: [" + sl::support::to_string(detach_err) + "]"));
//                    }
                });
            } else {
                throw jni_exception(TRACEMSG("JNI 'AttachCurrentThread' error code: [" + sl::support::to_string(attach_err) + "]"));
            }
        }
        default:
            throw jni_exception(TRACEMSG("JNI 'GetEnv' error code: [" + sl::support::to_string(getenv_err) + "]"));
        }
    }()) { }
    
    thread_local_jni_ptr(const thread_local_jni_ptr&) = delete;
    
    thread_local_jni_ptr& operator=(const thread_local_jni_ptr&) = delete;
    
    thread_local_jni_ptr(thread_local_jni_ptr&& other) :
    jni(std::move(other.jni)) { }
        
    thread_local_jni_ptr& operator=(thread_local_jni_ptr&& other) {
        jni = std::move(other.jni);
        return *this;
    }

    JNIEnv* operator->() {
        return jni.get();
    }

    JNIEnv* get() {
        return jni.get();
    }
};


class jclass_ptr {
    std::string clsname;
    std::shared_ptr<_jclass> cls;

public:
    jclass_ptr(const std::string& classname) :
    clsname(classname.data(), classname.length()),
    cls([this] {
        auto env = thread_local_jni_ptr();
        jclass local = env->FindClass(clsname.c_str());
        if (nullptr == local) {
            throw jni_exception(TRACEMSG("Cannot load class, name: [" + clsname + "]"));
        }
        jclass global = static_cast<jclass> (env->NewGlobalRef(local));
        if (nullptr == global) {
            throw jni_exception(TRACEMSG("Cannot create global ref for class, name: [" + clsname + "]"));
        }
        env->DeleteLocalRef(local);
        return std::shared_ptr<_jclass>(global, [](jclass clazz) {
            auto env = thread_local_jni_ptr();
            env->DeleteGlobalRef(clazz);
        });
    }()) { }

    jclass_ptr(const jclass_ptr& other) :
    clsname(other.clsname.data(), other.clsname.length()),
    cls(other.cls) { }

    jclass_ptr& operator=(const jclass_ptr& other) {
        clsname = std::string(other.clsname.data(), other.clsname.length());
        cls = other.cls;
        return *this;
    }

    jclass operator->() {
        return cls.get();
    }

    jclass get() {
        return cls.get();
    }

    template<typename Result, typename Func, typename... Args>
    Result call_static_method(const std::string& methodname, const std::string& signature,
            Func func, Args... args) {
        auto env = thread_local_jni_ptr();
        jmethodID method = env->GetStaticMethodID(cls.get(), methodname.c_str(), signature.c_str());
        if (nullptr == method) {
            throw jni_exception(TRACEMSG("Cannot find method, name: [" + methodname + "]," +
                    " signature: [" + signature + "], class: [" + clsname + "]"));
        }
        Result res = (env.get()->*func)(cls.get(), method, args...);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            throw jni_exception(TRACEMSG("Exception raised when calling method,"
                    " name: [" + methodname + "], signature: [" + signature + "], class: [" + clsname + "]"));
        }
        return res;
    }

    const std::string& name() {
        return clsname;
    }
};


class jobject_ptr {
    jclass_ptr cls;
    std::shared_ptr<_jobject> obj;

public:
    jobject_ptr(jclass_ptr clazz, jobject local) :
    cls(clazz),
    obj([this, local] {
        auto env = thread_local_jni_ptr();
        // let's play safe now and measure later
        jobject global = static_cast<jobject> (env->NewGlobalRef(local));
        if (nullptr == global) {
            throw jni_exception(TRACEMSG("Cannot create global ref for object, class name: [" + cls.name() + "]"));
        }
        env->DeleteLocalRef(local);
        return std::shared_ptr<_jobject>(global, [](jobject obj) {
            auto env = thread_local_jni_ptr();
            env->DeleteLocalRef(obj);
        });
    }()) { }

    jobject_ptr(const jobject_ptr& other) :
    cls(other.cls),
    obj(other.obj) { }

    jobject_ptr& operator=(const jobject_ptr& other) {
        cls = other.cls;
        obj = other.obj;
        return *this;
    }

    jobject operator->() {
        return obj.get();
    }

    jobject get() {
        return obj.get();
    }

    template<typename Result, typename Func, typename... Args>
    Result call_method(const std::string& methodname, const std::string& signature,
            Func func, Args... args) {
        auto env = thread_local_jni_ptr();
        jmethodID method = env->GetMethodID(cls.get(), methodname.c_str(), signature.c_str());
        if (nullptr == method) {
            throw jni_exception(TRACEMSG("Cannot find method, name: [" + methodname + "]," +
                    " signature: [" + signature + "], class: [" + cls.name() + "]"));
        }
        Result res = (env.get()->*func)(obj.get(), method, args...);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            throw jni_exception(TRACEMSG("Exception raised when calling method,"
                    " name: [" + methodname + "], signature: [" + signature + "], class: [" + cls.name() + "]"));
        }
        return res;
    }
};


java_vm_ptr& static_java_vm(JavaVM* jvm) {
    static java_vm_ptr vm{jvm};
    return vm;
}

} // namespace

namespace jvmti_helper {

class jvmti_error_checker {
public:
    void operator=(jvmtiError err) {
        if (JVMTI_ERROR_NONE != err) {
            throw jvmti_exception(TRACEMSG("JVMTI error code: [" + sl::support::to_string(err) + "]"));
        }
    }
};

sl::support::observer_ptr<jvmtiEnv> static_jvmti() {
    static sl::support::observer_ptr<jvmtiEnv> jvmti = [] {
        jni_helper::jni_error_checker ec;
        jvmtiEnv* env;
        ec = jni_helper::static_java_vm()->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JVMTI_VERSION);
        return sl::support::make_observer_ptr(env);
    }();
    return jvmti;
}


template<typename T> class jvmti_base {
protected:    
    std::string options;    
    std::thread worker;
    
    jvmti_base(JavaVM* javavm, char* options) :
    options(nullptr != options ? options : "") {
        jni_helper::static_java_vm(javavm);
        register_vminit_callback();
        apply_capabilities();
        // start worker
        this->worker = std::thread([this] {
            // wait for init
            jni_helper::static_java_vm().await_init_complete();
            // call inheritor
            static_cast<T*> (this)->operator()();
        });
    }
    
    jvmti_base(const jvmti_base&) = delete;
    
    void operator=(const jvmti_base&) = delete;
    
    // virtual is unnecessary
    ~jvmti_base() STATICLIB_NOEXCEPT {
        std::cout << "Shutting down ..." << std::endl;
        jni_helper::static_java_vm().notify_shutdown();
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
        ec = static_jvmti()->AddCapabilities(caps.get());
    }
    
    void register_vminit_callback() {
        auto jvmti = static_jvmti();
        jvmtiEventCallbacks cbs;
        memset(std::addressof(cbs), 0, sizeof (cbs));
        cbs.VMInit = [](jvmtiEnv*, JNIEnv*, jthread) {
            jni_helper::static_java_vm().notify_init_complete();
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
            while(jni_helper::static_java_vm().running()) {
                print_mem();
                jni_helper::static_java_vm().thread_sleep_before_shutdown(std::chrono::milliseconds(5000));
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
        auto rt = jni_helper::jclass_ptr("java/lang/Runtime");
        auto obj = rt.call_static_method<jobject>("getRuntime", "()Ljava/lang/Runtime;", &JNIEnv::CallStaticObjectMethod);
        auto jobj = jni_helper::jobject_ptr(rt, obj);
        auto res = jobj.call_method<jlong>("totalMemory", "()J", &JNIEnv::CallLongMethod);
        
        std::string path = "/proc/self/status";
        auto src = sl::tinydir::file_source(path);
        auto sink = sl::io::streambuf_sink(std::cout.rdbuf());
        sl::io::copy_all(src, sink);
        std::cout << std::endl << std::endl;
        std::cout << res << std::endl;
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
