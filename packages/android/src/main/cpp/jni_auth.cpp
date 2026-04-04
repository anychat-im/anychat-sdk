#include "jni_helpers.h"
#include "anychat_c.h"

using namespace anychat::jni;

extern JavaVM* g_jvm;

// Auth callback wrapper
static void authCallback(void* userdata, int success, const AnyChatAuthToken_C* token, const char* error) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(cls, "onAuthResult",
        "(ZLcom/anychat/sdk/models/AuthToken;Ljava/lang/String;)V");

    if (mid) {
        jobject tokenObj = nullptr;
        if (success && token) {
            tokenObj = convertAuthToken(env, *token);
        }
        jstring errorStr = toJString(env, error);
        env->CallVoidMethod(ctx->callback, mid, (jboolean)success, tokenObj, errorStr);

        if (tokenObj) env->DeleteLocalRef(tokenObj);
        if (errorStr) env->DeleteLocalRef(errorStr);
    }

    env->DeleteLocalRef(cls);
    delete ctx; // Clean up callback context
}

// Result callback wrapper
static void resultCallback(void* userdata, int success, const char* error) {
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

// Login
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeLogin(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring account,
    jstring password,
    jstring deviceType,
    jstring clientVersion,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper accountStr(env, account);
    JStringWrapper passwordStr(env, password);
    JStringWrapper deviceTypeStr(env, deviceType);
    JStringWrapper clientVersionStr(env, clientVersion);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_login(
        authHandle,
        accountStr.c_str(),
        passwordStr.c_str(),
        deviceTypeStr.c_str(),
        clientVersionStr.c_str(),
        ctx,
        authCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Login failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Register
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeRegister(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring phoneOrEmail,
    jstring password,
    jstring verifyCode,
    jstring deviceType,
    jstring nickname,
    jstring clientVersion,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper phoneOrEmailStr(env, phoneOrEmail);
    JStringWrapper passwordStr(env, password);
    JStringWrapper verifyCodeStr(env, verifyCode);
    JStringWrapper deviceTypeStr(env, deviceType);
    JStringWrapper nicknameStr(env, nickname);
    JStringWrapper clientVersionStr(env, clientVersion);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_register(
        authHandle,
        phoneOrEmailStr.c_str(),
        passwordStr.c_str(),
        verifyCodeStr.c_str(),
        deviceTypeStr.c_str(),
        nicknameStr.c_str(),
        clientVersionStr.c_str(),
        ctx,
        authCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Register failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Logout
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeLogout(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_logout(authHandle, ctx, resultCallback);

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Logout failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Refresh token
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeRefreshToken(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring refreshToken,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper refreshTokenStr(env, refreshToken);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_refresh_token(
        authHandle,
        refreshTokenStr.c_str(),
        ctx,
        authCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Refresh token failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Change password
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeChangePassword(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring oldPassword,
    jstring newPassword,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper oldPasswordStr(env, oldPassword);
    JStringWrapper newPasswordStr(env, newPassword);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_change_password(
        authHandle,
        oldPasswordStr.c_str(),
        newPasswordStr.c_str(),
        ctx,
        resultCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Change password failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Is logged in
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_anychat_sdk_Auth_nativeIsLoggedIn(JNIEnv* env, jobject thiz, jlong handle) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    int isLoggedIn = anychat_auth_is_logged_in(authHandle);
    return (jboolean)isLoggedIn;

    JNI_CATCH(env)
    return JNI_FALSE;
}

// Get current token
extern "C"
JNIEXPORT jobject JNICALL
Java_com_anychat_sdk_Auth_nativeGetCurrentToken(JNIEnv* env, jobject thiz, jlong handle) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    AnyChatAuthToken_C token = {};

    int result = anychat_auth_get_current_token(authHandle, &token);
    if (result == ANYCHAT_OK) {
        return convertAuthToken(env, token);
    }

    return nullptr;

    JNI_CATCH(env)
    return nullptr;
}
