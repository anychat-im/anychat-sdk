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

// Verification code callback wrapper
static void verificationCodeCallback(
    void* userdata,
    int success,
    const AnyChatVerificationCodeResult_C* result,
    const char* error
) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(
        cls,
        "onVerificationCodeResult",
        "(ZLcom/anychat/sdk/models/VerificationCodeResult;Ljava/lang/String;)V"
    );

    if (mid) {
        jobject resultObj = nullptr;
        if (success && result) {
            resultObj = convertVerificationCodeResult(env, *result);
        }
        jstring errorStr = toJString(env, error);
        env->CallVoidMethod(ctx->callback, mid, (jboolean)success, resultObj, errorStr);

        if (resultObj) env->DeleteLocalRef(resultObj);
        if (errorStr) env->DeleteLocalRef(errorStr);
    }

    env->DeleteLocalRef(cls);
    delete ctx;
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

// Auth device list callback wrapper
static void authDeviceListCallback(void* userdata, const AnyChatAuthDeviceList_C* list, const char* error) {
    auto* ctx = static_cast<CallbackContext*>(userdata);
    if (!ctx || !ctx->callback) return;

    JNIEnv* env = getEnvForCallback(ctx->jvm);
    if (!env) return;

    jclass cls = env->GetObjectClass(ctx->callback);
    jmethodID mid = env->GetMethodID(cls, "onAuthDeviceList", "(Ljava/util/List;Ljava/lang/String;)V");

    if (mid) {
        jobject devicesObj = nullptr;
        if (!error && list) {
            devicesObj = convertAuthDeviceList(env, list);
        }
        jstring errorStr = toJString(env, error);
        env->CallVoidMethod(ctx->callback, mid, devicesObj, errorStr);

        if (devicesObj) env->DeleteLocalRef(devicesObj);
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

// Send verification code
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeSendCode(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring target,
    jstring targetType,
    jstring purpose,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper targetStr(env, target);
    JStringWrapper targetTypeStr(env, targetType);
    JStringWrapper purposeStr(env, purpose);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_send_code(
        authHandle,
        targetStr.c_str(),
        targetTypeStr.c_str(),
        purposeStr.c_str(),
        ctx,
        verificationCodeCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Send code failed with error code: %d", result);
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

// Reset password
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeResetPassword(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring account,
    jstring verifyCode,
    jstring newPassword,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper accountStr(env, account);
    JStringWrapper verifyCodeStr(env, verifyCode);
    JStringWrapper newPasswordStr(env, newPassword);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_reset_password(
        authHandle,
        accountStr.c_str(),
        verifyCodeStr.c_str(),
        newPasswordStr.c_str(),
        ctx,
        resultCallback
    );

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Reset password failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Get auth device list
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeGetDeviceList(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_get_device_list(authHandle, ctx, authDeviceListCallback);

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Get device list failed with error code: %d", result);
    }

    JNI_CATCH(env)
}

// Logout specified device
extern "C"
JNIEXPORT void JNICALL
Java_com_anychat_sdk_Auth_nativeLogoutDevice(
    JNIEnv* env,
    jobject thiz,
    jlong handle,
    jstring deviceId,
    jobject callback
) {
    JNI_TRY(env)

    auto authHandle = reinterpret_cast<AnyChatAuthHandle>(handle);
    JStringWrapper deviceIdStr(env, deviceId);

    jobject globalCallback = env->NewGlobalRef(callback);
    auto* ctx = new CallbackContext(g_jvm, globalCallback);

    int result = anychat_auth_logout_device(authHandle, deviceIdStr.c_str(), ctx, resultCallback);

    if (result != ANYCHAT_OK) {
        delete ctx;
        env->DeleteGlobalRef(globalCallback);
        LOGE("Logout device failed with error code: %d", result);
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
