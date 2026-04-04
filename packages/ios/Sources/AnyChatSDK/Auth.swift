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

    init(handle: AnyChatAuthHandle) {
        self.handle = handle
        setupCallbacks()
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

            let callback: AnyChatAuthCallback = { userdata, success, token, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Login failed"
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

            let callback: AnyChatAuthCallback = { userdata, success, token, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Registration failed"
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
                                    let ctx = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()
                                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                                }
                            }
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

            let callback: AnyChatResultCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Logout failed"
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

            let callback: AnyChatAuthCallback = { userdata, success, token, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<AuthToken>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let token = token?.pointee {
                    context.continuation.resume(returning: AuthToken(from: token))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Token refresh failed"
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

            let callback: AnyChatResultCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Password change failed"
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
        }
    }

    // MARK: - Private

    private func setupCallbacks() {
        let callback: AnyChatAuthExpiredCallback = { userdata in
            guard let userdata = userdata else { return }
            let context = Unmanaged<StreamContext<Void>>.fromOpaque(userdata).takeUnretainedValue()
            context.continuation.yield(())
        }

        let context = StreamContext(continuation: tokenExpiredContinuation!)
        let userdata = Unmanaged.passRetained(context).toOpaque()

        anychat_auth_set_on_expired(handle, userdata, callback)
    }
}
