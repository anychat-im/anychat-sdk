//
//  Call.swift
//  AnyChatSDK
//
//  Call (Voice/Video) manager with async/await support
//

import Foundation

public actor CallManager {
    private let handle: AnyChatCallHandle
    private var incomingCallContinuation: AsyncStream<CallSession>.Continuation?
    private var callStatusChangedContinuation: AsyncStream<(String, CallStatus)>.Continuation?

    init(handle: AnyChatCallHandle) {
        self.handle = handle
        setupCallbacks()
    }

    // MARK: - One-to-One Call Operations

    public func initiateCall(
        calleeId: String,
        callType: CallType
    ) async throws -> CallSession {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallCallback = { userdata, success, session, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let session = session?.pointee {
                    context.continuation.resume(returning: CallSession(from: session))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Call initiation failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(calleeId) { calleePtr in
                let result = anychat_call_initiate_call(
                    handle,
                    calleePtr,
                    Int32(callType.rawValue),
                    userdata,
                    callback
                )

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func joinCall(callId: String) async throws -> CallSession {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallCallback = { userdata, success, session, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let session = session?.pointee {
                    context.continuation.resume(returning: CallSession(from: session))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Join call failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(callId) { callIdPtr in
                let result = anychat_call_join_call(handle, callIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func rejectCall(callId: String) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallResultCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Reject call failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(callId) { callIdPtr in
                let result = anychat_call_reject_call(handle, callIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func endCall(callId: String) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallResultCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "End call failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(callId) { callIdPtr in
                let result = anychat_call_end_call(handle, callIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func getCallSession(callId: String) async throws -> CallSession {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallCallback = { userdata, success, session, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let session = session?.pointee {
                    context.continuation.resume(returning: CallSession(from: session))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Get call session failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(callId) { callIdPtr in
                let result = anychat_call_get_call_session(handle, callIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<CallSession>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func getCallLogs(
        page: Int = 1,
        pageSize: Int = 20
    ) async throws -> ([CallSession], Int64) {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallListCallback = { userdata, list, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<([CallSession], Int64)>>.fromOpaque(userdata).takeRetainedValue()

                if let list = list {
                    let result = convertCallList(list)
                    context.continuation.resume(returning: result)
                    var mutableList = list.pointee
                    mutableList.free()
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Get call logs failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            let result = anychat_call_get_call_logs(
                handle,
                Int32(page),
                Int32(pageSize),
                userdata,
                callback
            )

            if result != ANYCHAT_OK {
                let ctx = Unmanaged<CallbackContext<([CallSession], Int64)>>.fromOpaque(userdata).takeRetainedValue()
                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
            }
        }
    }

    // MARK: - Meeting Operations

    public func createMeeting(
        title: String,
        password: String? = nil,
        maxParticipants: Int = 50
    ) async throws -> MeetingRoom {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatMeetingCallback = { userdata, success, room, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let room = room?.pointee {
                    context.continuation.resume(returning: MeetingRoom(from: room))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Create meeting failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(title) { titlePtr in
                withOptionalCString(password) { passwordPtr in
                    let result = anychat_call_create_meeting(
                        handle,
                        titlePtr,
                        passwordPtr,
                        Int32(maxParticipants),
                        userdata,
                        callback
                    )

                    if result != ANYCHAT_OK {
                        let ctx = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()
                        ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                    }
                }
            }
        }
    }

    public func joinMeeting(
        roomId: String,
        password: String? = nil
    ) async throws -> MeetingRoom {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatMeetingCallback = { userdata, success, room, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let room = room?.pointee {
                    context.continuation.resume(returning: MeetingRoom(from: room))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Join meeting failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(roomId) { roomIdPtr in
                withOptionalCString(password) { passwordPtr in
                    let result = anychat_call_join_meeting(
                        handle,
                        roomIdPtr,
                        passwordPtr,
                        userdata,
                        callback
                    )

                    if result != ANYCHAT_OK {
                        let ctx = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()
                        ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                    }
                }
            }
        }
    }

    public func endMeeting(roomId: String) async throws {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatCallResultCallback = { userdata, success, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0 {
                    context.continuation.resume(returning: ())
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "End meeting failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(roomId) { roomIdPtr in
                let result = anychat_call_end_meeting(handle, roomIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<Void>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func getMeeting(roomId: String) async throws -> MeetingRoom {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatMeetingCallback = { userdata, success, room, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()

                if success != 0, let room = room?.pointee {
                    context.continuation.resume(returning: MeetingRoom(from: room))
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "Get meeting failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            withCString(roomId) { roomIdPtr in
                let result = anychat_call_get_meeting(handle, roomIdPtr, userdata, callback)

                if result != ANYCHAT_OK {
                    let ctx = Unmanaged<CallbackContext<MeetingRoom>>.fromOpaque(userdata).takeRetainedValue()
                    ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
                }
            }
        }
    }

    public func listMeetings(
        page: Int = 1,
        pageSize: Int = 20
    ) async throws -> ([MeetingRoom], Int64) {
        try await withCheckedThrowingContinuation { continuation in
            let context = CallbackContext(continuation: continuation)
            let userdata = Unmanaged.passRetained(context).toOpaque()

            let callback: AnyChatMeetingListCallback = { userdata, list, error in
                guard let userdata = userdata else { return }
                let context = Unmanaged<CallbackContext<([MeetingRoom], Int64)>>.fromOpaque(userdata).takeRetainedValue()

                if let list = list {
                    let result = convertMeetingList(list)
                    context.continuation.resume(returning: result)
                    var mutableList = list.pointee
                    mutableList.free()
                } else {
                    let errorMsg = error != nil ? String(cString: error!) : "List meetings failed"
                    context.continuation.resume(throwing: AnyChatError.network)
                }
            }

            let result = anychat_call_list_meetings(
                handle,
                Int32(page),
                Int32(pageSize),
                userdata,
                callback
            )

            if result != ANYCHAT_OK {
                let ctx = Unmanaged<CallbackContext<([MeetingRoom], Int64)>>.fromOpaque(userdata).takeRetainedValue()
                ctx.continuation.resume(throwing: AnyChatError(code: Int(result)))
            }
        }
    }

    // MARK: - Event Streams

    public var incomingCall: AsyncStream<CallSession> {
        AsyncStream { continuation in
            self.incomingCallContinuation = continuation
        }
    }

    public var callStatusChanged: AsyncStream<(String, CallStatus)> {
        AsyncStream { continuation in
            self.callStatusChangedContinuation = continuation
        }
    }

    // MARK: - Private

    private func setupCallbacks() {
        let incomingCallback: AnyChatIncomingCallCallback = { userdata, session in
            guard let userdata = userdata, let session = session else { return }
            let context = Unmanaged<StreamContext<CallSession>>.fromOpaque(userdata).takeUnretainedValue()
            context.continuation.yield(CallSession(from: session.pointee))
        }

        let statusCallback: AnyChatCallStatusChangedCallback = { userdata, callId, status in
            guard let userdata = userdata, let callId = callId else { return }
            let context = Unmanaged<StreamContext<(String, CallStatus)>>.fromOpaque(userdata).takeUnretainedValue()
            let id = String(cString: callId)
            if let callStatus = CallStatus(rawValue: Int(status)) {
                context.continuation.yield((id, callStatus))
            }
        }

        Task {
            if let cont = incomingCallContinuation {
                let context = StreamContext(continuation: cont)
                let userdata = Unmanaged.passRetained(context).toOpaque()
                anychat_call_set_incoming_call_callback(handle, userdata, incomingCallback)
            }

            if let cont = callStatusChangedContinuation {
                let context = StreamContext(continuation: cont)
                let userdata = Unmanaged.passRetained(context).toOpaque()
                anychat_call_set_call_status_changed_callback(handle, userdata, statusCallback)
            }
        }
    }
}
