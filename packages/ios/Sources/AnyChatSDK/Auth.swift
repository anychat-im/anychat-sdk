//
//  Auth.swift
//  AnyChatSDK
//
//  Authentication manager with async/await support
//

import Foundation

public actor AuthManager {
    private let handle: AnyChatAuthHandle
    private var tokenExpiredContinuation: AsyncStream<Void>.Continuation?
    private var tokenExpiredContext: UnsafeMutableRawPointer?

    init(handle: AnyChatAuthHandle) {
        self.handle = handle
    }

    deinit {
        anychat_auth_set_on_expired(handle, nil, nil)
        if let context = tokenExpiredContext {
            Unmanaged<StreamContext<Void>>.fromOpaque(context).release()
            tokenExpiredContext = nil
        }
    }

    // MARK: - Authentication Operations

    public func login(
        account: String,
        password: String,
        deviceType: String = "ios",
        clientVersion: String = ""
    ) async throws -> AuthToken {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatAuthCallback = { userdata, success, token, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(account) { accountPtr in
                withCString(password) { passwordPtr in
                    withCString(deviceType) { deviceTypePtr in
                        withOptionalCString(clientVersion.isEmpty ? nil : clientVersion) { clientVersionPtr in
                            let result = anychat_auth_login(
                                handle,
                                accountPtr,
                                passwordPtr,
                                deviceTypePtr,
                                clientVersionPtr,
                                userdata,
                                callback
                            )

                            if result != ANYCHAT_OK {
                                let ctx = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()
                                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                            }
                        }
                    }
                }
            }
        }
    }

    public func register(
        phoneOrEmail: String,
        password: String,
        verifyCode: String,
        deviceType: String = "ios",
        nickname: String? = nil,
        clientVersion: String = ""
    ) async throws -> AuthToken {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatAuthCallback = { userdata, success, token, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(phoneOrEmail) { phonePtr in
                withCString(password) { passwordPtr in
                    withCString(verifyCode) { codePtr in
                        withCString(deviceType) { deviceTypePtr in
                            withOptionalCString(nickname) { nicknamePtr in
                                withOptionalCString(clientVersion.isEmpty ? nil : clientVersion) { clientVersionPtr in
                                    let result = anychat_auth_register(
                                        handle,
                                        phonePtr,
                                        passwordPtr,
                                        codePtr,
                                        deviceTypePtr,
                                        nicknamePtr,
                                        clientVersionPtr,
                                        userdata,
                                        callback
                                    )

                                    if result != ANYCHAT_OK {
                                        let ctx = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata)
                                            .takeRetainedValue()
                                        ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    public func sendCode(
        target: String,
        targetType: String,
        purpose: String
    ) async throws -> VerificationCodeResult {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatSendCodeCallback = { userdata, success, result, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<VerificationCodeResult>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let result = result?.pointee {
                    context.continuation.resume(returning: VerificationCodeResult(from: result))
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(target) { targetPtr in
                withCString(targetType) { targetTypePtr in
                    withCString(purpose) { purposePtr in
                        let result = anychat_auth_send_code(
                            handle,
                            targetPtr,
                            targetTypePtr,
                            purposePtr,
                            userdata,
                            callback
                        )

                        if result != ANYCHAT_OK {
                            let ctx = Unmanaged<CallbackContext<VerificationCodeResult>>.fromOpaque(userdata)
                                .takeRetainedValue()
                            ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                        }
                    }
                }
            }
        }
    }

    public func logout() async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatResultCallback = { userdata, success, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            let result = anychat_auth_logout(handle, userdata, callback)
            if result != ANYCHAT_OK {
                let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
            }
        }
    }

    public func refreshToken(_ refreshToken: String) async throws -> AuthToken {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatAuthCallback = { userdata, success, token, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    context.continuation.resume(throwing: AnyChatError.tokenExpired)
                }
            }

            withCString(refreshToken) { tokenPtr in
                let result = anychat_auth_refresh_token(
                    handle,
                    tokenPtr,
                    userdata,
                    callback
                )

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func changePassword(
        oldPassword: String,
        newPassword: String
    ) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatResultCallback = { userdata, success, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(oldPassword) { oldPtr in
                withCString(newPassword) { newPtr in
                    let result = anychat_auth_change_password(
                        handle,
                        oldPtr,
                        newPtr,
                        userdata,
                        callback
                    )

                    if result != ANYCHAT_OK {
                        let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                        ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                    }
                }
            }
        }
    }

    public func resetPassword(
        account: String,
        verifyCode: String,
        newPassword: String
    ) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatResultCallback = { userdata, success, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(account) { accountPtr in
                withCString(verifyCode) { verifyCodePtr in
                    withCString(newPassword) { newPasswordPtr in
                        let result = anychat_auth_reset_password(
                            handle,
                            accountPtr,
                            verifyCodePtr,
                            newPasswordPtr,
                            userdata,
                            callback
                        )

                        if result != ANYCHAT_OK {
                            let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                            ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                        }
                    }
                }
            }
        }
    }

    public func getDeviceList() async throws -> [AuthDevice] {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatAuthDeviceListCallback = { userdata, list, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<[AuthDevice]>>.fromOpaque(userdata).takeRetainedValue()

                if error == nil, let list = list {
                    context.continuation.resume(returning: convertAuthDeviceList(list))
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            let result = anychat_auth_get_device_list(handle, userdata, callback)
            if result != ANYCHAT_OK {
                let ctx = Unmanaged<CallbackContext<[AuthDevice]>>.fromOpaque(userdata).takeRetainedValue()
                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
            }
        }
    }

    public func logoutDevice(deviceId: String) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatResultCallback = { userdata, success, _ in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    context.continuation.resume(throwing: AnyChatError.auth)
                }
            }

            withCString(deviceId) { deviceIdPtr in
                let result = anychat_auth_logout_device(handle, deviceIdPtr, userdata, callback)
                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    // MARK: - State Queries

    public func isLoggedIn() -> Bool {
        return anychat_auth_is_logged_in(handle) != 0
    }

    public func getCurrentToken() throws -> AuthToken {
        var cToken = AnyChatAuthToken_C()
        let result = anychat_auth_get_current_token(handle, &cToken)
        try checkResult(result)
        return AuthToken(from: cToken)
    }

    // MARK: - Event Streams

    public var tokenExpired: AsyncStream<Void> {
        AsyncStream { continuation in
            self.tokenExpiredContinuation = continuation
            self.setupCallbacks()
        }
    }

    // MARK: - Private

    private func setupCallbacks() {
        guard let continuation = tokenExpiredContinuation else {
            return
        }

        anychat_auth_set_on_expired(handle, nil, nil)
        if let context = tokenExpiredContext {
            Unmanaged<StreamContext<Void>>.fromOpaque(context).release()
            tokenExpiredContext = nil
        }

        let callback: AnyChatAuthExpiredCallback = { userdata in
            guard let userdata = userdata else { return }
            let context = Unmanaged<StreamContext<Void>>.fromOpaque(userdata).takeUnretainedValue()
            context.continuation.yield(())
        }

        let context = StreamContext(continuation: continuation)
        let userdata = Unmanaged.passRetained(context).toOpaque()
        tokenExpiredContext = userdata
        anychat_auth_set_on_expired(handle, userdata, callback)
    }
}
