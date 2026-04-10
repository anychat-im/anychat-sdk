#pragma once

#include <jni.h>
#include <string>
#include <memory>
#include <android/log.h>

// Android logging macros
#define LOG_TAG "AnyChatJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// JNI exception handling
#define JNI_TRY(env) \
    try {

#define JNI_CATCH(env) \
    } catch (const std::exception& e) { \
        jclass exClass = env->FindClass("java/lang/RuntimeException"); \
        if (exClass != nullptr) { \
            env->ThrowNew(exClass, e.what()); \
        } \
    } catch (...) { \
        jclass exClass = env->FindClass("java/lang/RuntimeException"); \
        if (exClass != nullptr) { \
            env->ThrowNew(exClass, "Unknown native exception"); \
        } \
    }

namespace anychat {
namespace jni {

// Helper class for managing JNI strings
class JStringWrapper {
public:
    JStringWrapper(JNIEnv* env, jstring jstr)
        : env_(env), jstr_(jstr), cstr_(nullptr) {
        if (jstr != nullptr) {
            cstr_ = env->GetStringUTFChars(jstr, nullptr);
        }
    }

    ~JStringWrapper() {
        if (cstr_ != nullptr) {
            env_->ReleaseStringUTFChars(jstr_, cstr_);
        }
    }

    const char* c_str() const { return cstr_ ? cstr_ : ""; }
    operator const char*() const { return c_str(); }

private:
    JNIEnv* env_;
    jstring jstr_;
    const char* cstr_;
};

// Helper to create Java strings
inline jstring toJString(JNIEnv* env, const char* str) {
    if (!str) return nullptr;
    return env->NewStringUTF(str);
}

// Helper to get field ID
inline jfieldID getFieldID(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = env->GetFieldID(cls, name, sig);
    env->DeleteLocalRef(cls);
    return fid;
}

// Helper to get method ID
inline jmethodID getMethodID(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    jclass cls = env->GetObjectClass(obj);
    jmethodID mid = env->GetMethodID(cls, name, sig);
    env->DeleteLocalRef(cls);
    return mid;
}

// Helper to get static method ID
inline jmethodID getStaticMethodID(JNIEnv* env, const char* className, const char* name, const char* sig) {
    jclass cls = env->FindClass(className);
    if (!cls) return nullptr;
    jmethodID mid = env->GetStaticMethodID(cls, name, sig);
    env->DeleteLocalRef(cls);
    return mid;
}

// Global reference manager for callbacks
class GlobalRefManager {
public:
    static GlobalRefManager& instance() {
        static GlobalRefManager inst;
        return inst;
    }

    jobject addGlobalRef(JNIEnv* env, jobject obj) {
        if (!obj) return nullptr;
        return env->NewGlobalRef(obj);
    }

    void deleteGlobalRef(JNIEnv* env, jobject obj) {
        if (obj) {
            env->DeleteGlobalRef(obj);
        }
    }

private:
    GlobalRefManager() = default;
};

// Struct to hold Java callback info
struct CallbackContext {
    JavaVM* jvm;
    jobject callback;  // GlobalRef

    CallbackContext(JavaVM* vm, jobject cb) : jvm(vm), callback(cb) {}

    ~CallbackContext() {
        if (callback) {
            JNIEnv* env = nullptr;
            if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
                env->DeleteGlobalRef(callback);
            }
        }
    }
};

// Helper to get JNIEnv from JavaVM (for callbacks on C threads)
inline JNIEnv* getEnvForCallback(JavaVM* jvm) {
    JNIEnv* env = nullptr;
    int status = jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        // Attach the current thread
        JavaVMAttachArgs args = {JNI_VERSION_1_6, nullptr, nullptr};
        if (jvm->AttachCurrentThread(&env, &args) != JNI_OK) {
            LOGE("Failed to attach thread");
            return nullptr;
        }
    } else if (status != JNI_OK) {
        LOGE("Failed to get JNIEnv");
        return nullptr;
    }

    return env;
}

// Convert C struct to Java object helpers
jobject convertAuthToken(JNIEnv* env, const AnyChatAuthToken_C& token);
jobject convertVerificationCodeResult(JNIEnv* env, const AnyChatVerificationCodeResult_C& result);
jobject convertAuthDevice(JNIEnv* env, const AnyChatAuthDevice_C& device);
jobject convertUserInfo(JNIEnv* env, const AnyChatUserInfo_C& info);
jobject convertMessage(JNIEnv* env, const AnyChatMessage_C& msg);
jobject convertConversation(JNIEnv* env, const AnyChatConversation_C& conv);
jobject convertFriend(JNIEnv* env, const AnyChatFriend_C& friendInfo);
jobject convertFriendRequest(JNIEnv* env, const AnyChatFriendRequest_C& request);
jobject convertGroup(JNIEnv* env, const AnyChatGroup_C& group);
jobject convertGroupMember(JNIEnv* env, const AnyChatGroupMember_C& member);

// Convert C list to Java List
jobject convertMessageList(JNIEnv* env, const AnyChatMessageList_C* list);
jobject convertConversationList(JNIEnv* env, const AnyChatConversationList_C* list);
jobject convertFriendList(JNIEnv* env, const AnyChatFriendList_C* list);
jobject convertGroupList(JNIEnv* env, const AnyChatGroupList_C* list);
jobject convertAuthDeviceList(JNIEnv* env, const AnyChatAuthDeviceList_C* list);

} // namespace jni
} // namespace anychat
