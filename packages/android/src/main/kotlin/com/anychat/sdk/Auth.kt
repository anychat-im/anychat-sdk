package com.anychat.sdk

import com.anychat.sdk.models.AuthToken
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.coroutines.suspendCoroutine

/**
 * Authentication manager
 *
 * Provides authentication-related operations including login, register, and token management.
 */
class Auth internal constructor(private val handle: Long) {

    /**
     * Login with account and password
     *
     * @param account Phone number or email
     * @param password User password
     * @param deviceType Device type: "android", "ios", "web"
     * @param clientVersion Client version string (e.g. "1.0.0")
     * @return AuthToken on success
     */
    suspend fun login(
        account: String,
        password: String,
        deviceType: String = "android",
        clientVersion: String = ""
    ): AuthToken = suspendCoroutine { continuation ->
        nativeLogin(handle, account, password, deviceType, clientVersion, object : AuthCallback {
            override fun onAuthResult(success: Boolean, token: AuthToken?, error: String?) {
                if (success && token != null) {
                    continuation.resume(token)
                } else {
                    continuation.resumeWithException(
                        RuntimeException(error ?: "Login failed")
                    )
                }
            }
        })
    }

    /**
     * Register a new account
     *
     * @param phoneOrEmail Phone number or email
     * @param password User password
     * @param verifyCode SMS/email verification code
     * @param deviceType Device type: "android", "ios", "web"
     * @param nickname Optional nickname
     * @param clientVersion Client version string (e.g. "1.0.0")
     * @return AuthToken on success
     */
    suspend fun register(
        phoneOrEmail: String,
        password: String,
        verifyCode: String,
        deviceType: String = "android",
        nickname: String? = null,
        clientVersion: String = ""
    ): AuthToken = suspendCoroutine { continuation ->
        nativeRegister(
            handle,
            phoneOrEmail,
            password,
            verifyCode,
            deviceType,
            nickname ?: "",
            clientVersion,
            object : AuthCallback {
                override fun onAuthResult(success: Boolean, token: AuthToken?, error: String?) {
                    if (success && token != null) {
                        continuation.resume(token)
                    } else {
                        continuation.resumeWithException(
                            RuntimeException(error ?: "Register failed")
                        )
                    }
                }
            })
    }

    /**
     * Logout current user
     */
    suspend fun logout(): Unit = suspendCoroutine { continuation ->
        nativeLogout(handle, object : ResultCallback {
            override fun onResult(success: Boolean, error: String?) {
                if (success) {
                    continuation.resume(Unit)
                } else {
                    continuation.resumeWithException(
                        RuntimeException(error ?: "Logout failed")
                    )
                }
            }
        })
    }

    /**
     * Refresh access token using refresh token
     *
     * @param refreshToken The refresh token
     * @return New AuthToken
     */
    suspend fun refreshToken(refreshToken: String): AuthToken = suspendCoroutine { continuation ->
        nativeRefreshToken(handle, refreshToken, object : AuthCallback {
            override fun onAuthResult(success: Boolean, token: AuthToken?, error: String?) {
                if (success && token != null) {
                    continuation.resume(token)
                } else {
                    continuation.resumeWithException(
                        RuntimeException(error ?: "Refresh token failed")
                    )
                }
            }
        })
    }

    /**
     * Change user password
     *
     * @param oldPassword Current password
     * @param newPassword New password
     */
    suspend fun changePassword(
        oldPassword: String,
        newPassword: String
    ): Unit = suspendCoroutine { continuation ->
        nativeChangePassword(handle, oldPassword, newPassword, object : ResultCallback {
            override fun onResult(success: Boolean, error: String?) {
                if (success) {
                    continuation.resume(Unit)
                } else {
                    continuation.resumeWithException(
                        RuntimeException(error ?: "Change password failed")
                    )
                }
            }
        })
    }

    /**
     * Check if user is currently logged in
     */
    val isLoggedIn: Boolean
        get() = nativeIsLoggedIn(handle)

    /**
     * Get current auth token
     *
     * @return Current token or null if not logged in
     */
    val currentToken: AuthToken?
        get() = nativeGetCurrentToken(handle)

    // Native methods
    private external fun nativeLogin(
        handle: Long,
        account: String,
        password: String,
        deviceType: String,
        clientVersion: String,
        callback: AuthCallback
    )

    private external fun nativeRegister(
        handle: Long,
        phoneOrEmail: String,
        password: String,
        verifyCode: String,
        deviceType: String,
        nickname: String,
        clientVersion: String,
        callback: AuthCallback
    )

    private external fun nativeLogout(handle: Long, callback: ResultCallback)

    private external fun nativeRefreshToken(
        handle: Long,
        refreshToken: String,
        callback: AuthCallback
    )

    private external fun nativeChangePassword(
        handle: Long,
        oldPassword: String,
        newPassword: String,
        callback: ResultCallback
    )

    private external fun nativeIsLoggedIn(handle: Long): Boolean
    private external fun nativeGetCurrentToken(handle: Long): AuthToken?
}
