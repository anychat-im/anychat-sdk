#include "jni_helpers.h"
#include "anychat_c.h"

using namespace anychat::jni;

extern JavaVM* g_jvm;

// Conversation list callback wrapper
static void convListCallback(void* userdata, const AnyChatConversationList_C* list, const char* error) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(cls, "onConversationList",
        "(Ljava/util/List;Ljava/lang/String;)V");

    if (mid) {
        jobject listObj = nullptr;
        if (list) {
            listObj = convertConversationList(env, list);
        }
        jstring errorStr = toJString(env, error);
        env->CallVoidMethod(ctx->callback, mid, listObj, errorStr);

        if (listObj) env->DeleteLocalRef(listObj);
        if (errorStr) env->DeleteLocalRef(errorStr);
    }

    env->DeleteLocalRef(cls);
    delete ctx;
}

// Conversation callback wrapper
static void convCallback(void* userdata, int success, const char* error) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(cls, "onResult", "(ZLjava/lang/String;)V");

    if (mid) {
        jstring errorStr = toJString(env, error);
        env->CallVoidMethod(ctx->callback, mid, (jboolean)success, errorStr);
        if (errorStr) env->DeleteLocalRef(errorStr);
    }

    env->DeleteLocalRef(cls);
    delete ctx;
}

// Conversation updated callback wrapper
static void convUpdatedCallback(void* userdata, const AnyChatConversation_C* conversation) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(cls, "onConversationUpdated",
        "(Lcom/anychat/sdk/models/Conversation;)V");

    if (mid) {
        jobject convObj = convertConversation(env, *conversation);
        if (convObj) {
            env->CallVoidMethod(ctx->callback, mid, convObj);
            env->DeleteLocalRef(convObj);
        }
    }

    env->DeleteLocalRef(cls);
}

// Get conversation list
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Conversation_nativeGetList(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jobject callback
) {
    JNI_TRY(env)

    auto convHandle = reinterpret_cast<AnyChatConvHandle>(handle);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_conv_get_list(convHandle, ctx, convListCallback);

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Get conversation list failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Set conversation pinned
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Conversation_nativeSetPinned(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring convId,
    jboolean pinned,
    jobject callback
) {
    JNI_TRY(env)

    auto convHandle = reinterpret_cast<AnyChatConvHandle>(handle);
    JStringWrapper convIdStr(env, convId);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_conv_set_pinned(
        convHandle,
        convIdStr.c_str(),
        pinned ? 1 : 0,
        ctx,
        convCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Set conversation pinned failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Set conversation muted
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Conversation_nativeSetMuted(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring convId,
    jboolean muted,
    jobject callback
) {
    JNI_TRY(env)

    auto convHandle = reinterpret_cast<AnyChatConvHandle>(handle);
    JStringWrapper convIdStr(env, convId);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_conv_set_muted(
        convHandle,
        convIdStr.c_str(),
        muted ? 1 : 0,
        ctx,
        convCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Set conversation muted failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Delete conversation
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Conversation_nativeDelete(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring convId,
    jobject callback
) {
    JNI_TRY(env)

    auto convHandle = reinterpret_cast<AnyChatConvHandle>(handle);
    JStringWrapper convIdStr(env, convId);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_conv_delete(convHandle, convIdStr.c_str(), ctx, convCallback);

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Delete conversation failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Set conversation updated callback
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Conversation_nativeSetUpdatedCallback(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jobject callback
) {
    JNI_TRY(env)

    auto convHandle = reinterpret_cast<AnyChatConvHandle>(handle);

    if (callback) {
        jobject globalCallback = env->NewGlobalRef(callback);
        auto* ctx = new CallbackContext(g_jvm, globalCallback);
        anychat_conv_set_updated_callback(convHandle, ctx, convUpdatedCallback);
    } else {
        anychat_conv_set_updated_callback(convHandle, nullptr, nullptr);
    }

    JNI_CATCH(env)
}
