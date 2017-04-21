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
#include <ios>

#include "staticlib/config.hpp"
#include "staticlib/concurrent.hpp"
#include "staticlib/io.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"

#include "jvmti_exception.hpp"

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


class java_class;

class java_vm {
    sl::support::observer_ptr<JavaVM> jvm;
    std::atomic<bool> shutdown_flag;

public:
    java_vm(JavaVM* vm) :
    jvm(vm),
    shutdown_flag(false) { }

    java_vm(const java_vm&) = delete;

    java_vm& operator=(const java_vm&) = delete;

    bool running() {
        return !shutdown_flag.load(std::memory_order_acquire);
    }

    void mark_shutted_down() {
        shutdown_flag.store(true, std::memory_order_release);
    }

    sl::support::observer_ptr<JavaVM> operator->() {
        return jvm;
    }

    sl::support::observer_ptr<JNIEnv> jni() {
        JNIEnv* env;
        auto getenv_err = jvm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JNI_VERSION_1_6);
        switch (getenv_err) {
        case JNI_OK:
            return sl::support::make_observer_ptr(env);
        case JNI_EDETACHED:
        {
            auto attach_err = jvm->AttachCurrentThread(reinterpret_cast<void**> (std::addressof(env)), nullptr);
            if (JNI_OK == attach_err) {
                return sl::support::make_observer_ptr(env);
            } else {
                throw jvmti_exception(TRACEMSG("JNI 'AttachCurrentThread' error code: [" + sl::support::to_string(attach_err) + "]"));
            }
        }
        default:
            throw jvmti_exception(TRACEMSG("JNI 'GetEnv' error code: [" + sl::support::to_string(getenv_err) + "]"));
        }
    }

    java_class find_class(const std::string& name);
};

class java_class_deleter {
    sl::support::observer_ptr<java_vm> jvm;

public:
    java_class_deleter(java_vm& javavm) :
    jvm(javavm) { }

    void operator()(jclass clazz) {
        jvm->jni()->DeleteGlobalRef(clazz);
    }
};


class java_class {
    sl::support::observer_ptr<java_vm> jvm;
    std::string clsname;
    std::shared_ptr<_jclass> cls;

public:
    java_class(java_vm& javavm, const std::string& classname) :
    jvm(javavm),
    clsname(classname.data(), classname.length()),
    cls([this] {
        auto env = jvm->jni();
        jclass local = env->FindClass(clsname.c_str());
        if (nullptr == local) {
            throw jvmti_exception(TRACEMSG("Cannot load class, name: [" + clsname + "]"));
        }
        jclass global = static_cast<jclass> (env->NewGlobalRef(local));
        if (nullptr == global) {
            throw jvmti_exception(TRACEMSG("Cannot create global ref for class, name: [" + clsname + "]"));
        }
        env->DeleteLocalRef(local);
        return std::shared_ptr<_jclass>(global, java_class_deleter(*jvm));
    }()) { }

    java_class(const java_class& other) :
    jvm(other.jvm),
    clsname(other.clsname.data(), other.clsname.length()),
    cls(other.cls) { }

    java_class& operator=(const java_class& other) {
        jvm = other.jvm;
        clsname = std::string(other.clsname.data(), other.clsname.length());
        cls = other.cls;
        return *this;
    }
    
    template<typename Result, typename Func, typename... Args>
    Result call_static_method(const std::string& methodname, const std::string& signature,
            Func func, Args... args) {
        auto env = jvm->jni();
        jmethodID method = env->GetStaticMethodID(cls.get(), methodname.c_str(), signature.c_str());
        if (nullptr == method) {
            throw jvmti_exception(TRACEMSG("Cannot find method, name: [" + methodname + "]," +
                    " signature: [" + signature + "], class: [" + clsname + "]"));
        }
        Result res = (env.get()->*func)(cls.get(), method, args...);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            throw jvmti_exception(TRACEMSG("Exception raised when calling method,"
                    " name: [" + methodname + "], signature: [" + signature + "], class: [" + clsname + "]"));
        }
        return res;
    }
    
    jclass get() {
        return cls.get();
    }
    
    const std::string& name() {
        return clsname;
    }
};

java_class java_vm::find_class(const std::string& name) {
    return java_class(*this, name);
}

class java_object_deleter {
    sl::support::observer_ptr<java_vm> jvm;

public:
    java_object_deleter(java_vm& javavm) :
    jvm(javavm) { }

    void operator()(jobject obj) {
        jvm->jni()->DeleteLocalRef(obj);
    }
};

class java_object {
    sl::support::observer_ptr<java_vm> jvm;
    java_class cls;
    std::shared_ptr<_jobject> obj;
    
public:
    java_object(java_vm& javavm, java_class clazz, jobject local) :
    jvm(javavm),
    cls(clazz),
    obj([this, local] {
        auto env = jvm->jni();
        // let's play safe now and measure later
        jobject global = static_cast<jobject> (env->NewGlobalRef(local));
        if (nullptr == global) {
            throw jvmti_exception(TRACEMSG("Cannot create global ref for object, class name: [" + cls.name() + "]"));
        }
        env->DeleteLocalRef(local);
        return std::shared_ptr<_jobject>(global, java_object_deleter(*jvm));
    }()) { }
    
    java_object(const java_object& other) :
    jvm(other.jvm),
    cls(other.cls),
    obj(other.obj) { }
    
    java_object& operator=(const java_object& other) {
        jvm = other.jvm;
        cls = other.cls;
        obj = other.obj;
        return *this;
    }

    template<typename Result, typename Func, typename... Args>
    Result call_method(const std::string& methodname, const std::string& signature,
            Func func, Args... args) {
        auto env = jvm->jni();
        jmethodID method = env->GetMethodID(cls.get(), methodname.c_str(), signature.c_str());
        if (nullptr == method) {
            throw jvmti_exception(TRACEMSG("Cannot find method, name: [" + methodname + "]," +
                    " signature: [" + signature + "], class: [" + cls.name() + "]"));
        }
        Result res = (env.get()->*func)(obj.get(), method, args...);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            throw jvmti_exception(TRACEMSG("Exception raised when calling method,"
                    " name: [" + methodname + "], signature: [" + signature + "], class: [" + cls.name() + "]"));
        }
        return res;
    }
};

template<typename T> 
class jvmti_ctx {
protected:    
    // TODO: make me global
    java_vm jvm;
    std::string options;
    sl::support::observer_ptr<jvmtiEnv> jvmti;
    // TODO: move me into java_vm
    sl::concurrent::countdown_latch vminit_latch;
    // TODO: move me into java_vm
    sl::concurrent::condition_latch waiting_latch;
    std::thread worker;
    
    jvmti_ctx(JavaVM* javavm, char* options) :
    jvm(javavm),
    options(nullptr != options ? options : ""),
    jvmti([this] {
        jni_error_checker ec;
        jvmtiEnv* env;
        ec = this->jvm->GetEnv(reinterpret_cast<void**> (std::addressof(env)), JVMTI_VERSION);
        return env;
    }()),
    vminit_latch(1),
    waiting_latch([this] {
        return !this->jvm.running();
    }) {
        register_vminit_callback();
        apply_capabilities();
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
        jvm.mark_shutted_down();
        waiting_latch.notify_all();
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
        // register init callback, latch is passed through global var
        jvmtiEventCallbacks cbs;
        memset(std::addressof(cbs), 0, sizeof (cbs));
        static_vminit_latch(std::addressof(vminit_latch));
        cbs.VMInit = [](jvmtiEnv*, JNIEnv*, jthread) {
            static_vminit_latch()->count_down();
        };
        jvmti_error_checker ec;
        ec = jvmti->SetEventCallbacks(std::addressof(cbs), sizeof (cbs));
        ec = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    }
};

} // namespace


class meminfo : public jvmti_helper::jvmti_ctx<meminfo> {
public:
    meminfo(JavaVM* jvm, char* options) :
    jvmti_helper::jvmti_ctx<meminfo>(jvm, options) { }
    
    void operator()() STATICLIB_NOEXCEPT {
        try {
            while(jvm.running()) {
                print_mem();
                waiting_latch.await(std::chrono::milliseconds(5000));
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
        auto rt = jvm.find_class("java/lang/Runtime");
        auto obj = rt.call_static_method<jobject>("getRuntime", "()Ljava/lang/Runtime;", &JNIEnv::CallStaticObjectMethod);
        auto jobj = jvmti_helper::java_object(jvm, rt, obj);
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
