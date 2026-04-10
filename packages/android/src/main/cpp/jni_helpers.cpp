#include "jni_helpers.h"
#include "anychat_c.h"
#include <vector>

namespace anychat {
namespace jni {

jobject convertAuthToken(JNIEnv* env, const AnyChatAuthToken_C& token) {
    jclass cls = env->FindClass("com/anychat/sdk/models/AuthToken");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;J)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring accessToken = toJString(env, token.access_token);
    jstring refreshToken = toJString(env, token.refresh_token);

    jobject obj = env->NewObject(cls, constructor, accessToken, refreshToken, (jlong)token.expires_at_ms);

    env->DeleteLocalRef(accessToken);
    env->DeleteLocalRef(refreshToken);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertVerificationCodeResult(JNIEnv* env, const AnyChatVerificationCodeResult_C& result) {
    jclass cls = env->FindClass("com/anychat/sdk/models/VerificationCodeResult");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;J)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring codeId = toJString(env, result.code_id);
    jobject obj = env->NewObject(cls, constructor, codeId, (jlong)result.expires_in);

    env->DeleteLocalRef(codeId);
    env->DeleteLocalRef(cls);
    return obj;
}

jobject convertAuthDevice(JNIEnv* env, const AnyChatAuthDevice_C& device) {
    jclass cls = env->FindClass("com/anychat/sdk/models/AuthDevice");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(
        cls,
        "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JZ)V"
    );
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring deviceId = toJString(env, device.device_id);
    jstring deviceType = toJString(env, device.device_type);
    jstring clientVersion = toJString(env, device.client_version);
    jstring lastLoginIp = toJString(env, device.last_login_ip);

    jobject obj = env->NewObject(
        cls,
        constructor,
        deviceId,
        deviceType,
        clientVersion,
        lastLoginIp,
        (jlong)device.last_login_at_ms,
        (jboolean)device.is_current
    );

    env->DeleteLocalRef(deviceId);
    env->DeleteLocalRef(deviceType);
    env->DeleteLocalRef(clientVersion);
    env->DeleteLocalRef(lastLoginIp);
    env->DeleteLocalRef(cls);
    return obj;
}

jobject convertUserInfo(JNIEnv* env, const AnyChatUserInfo_C& info) {
    jclass cls = env->FindClass("com/anychat/sdk/models/UserInfo");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring userId = toJString(env, info.user_id);
    jstring username = toJString(env, info.username);
    jstring avatarUrl = toJString(env, info.avatar_url);

    jobject obj = env->NewObject(cls, constructor, userId, username, avatarUrl);

    env->DeleteLocalRef(userId);
    env->DeleteLocalRef(username);
    env->DeleteLocalRef(avatarUrl);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertMessage(JNIEnv* env, const AnyChatMessage_C& msg) {
    jclass cls = env->FindClass("com/anychat/sdk/models/Message");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;JLjava/lang/String;JIIZ)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring messageId = toJString(env, msg.message_id);
    jstring localId = toJString(env, msg.local_id);
    jstring convId = toJString(env, msg.conv_id);
    jstring senderId = toJString(env, msg.sender_id);
    jstring contentType = toJString(env, msg.content_type);
    jstring content = toJString(env, msg.content);
    jstring replyTo = toJString(env, msg.reply_to);

    jobject obj = env->NewObject(cls, constructor,
        messageId, localId, convId, senderId, contentType,
        (jint)msg.type, content, (jlong)msg.seq, replyTo,
        (jlong)msg.timestamp_ms, (jint)msg.status, (jint)msg.send_state,
        (jboolean)msg.is_read);

    env->DeleteLocalRef(messageId);
    env->DeleteLocalRef(localId);
    env->DeleteLocalRef(convId);
    env->DeleteLocalRef(senderId);
    env->DeleteLocalRef(contentType);
    env->DeleteLocalRef(content);
    env->DeleteLocalRef(replyTo);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertConversation(JNIEnv* env, const AnyChatConversation_C& conv) {
    jclass cls = env->FindClass("com/anychat/sdk/models/Conversation");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;JIZZJ)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring convId = toJString(env, conv.conv_id);
    jstring targetId = toJString(env, conv.target_id);
    jstring lastMsgId = toJString(env, conv.last_msg_id);
    jstring lastMsgText = toJString(env, conv.last_msg_text);

    jobject obj = env->NewObject(cls, constructor,
        convId, (jint)conv.conv_type, targetId, lastMsgId, lastMsgText,
        (jlong)conv.last_msg_time_ms, (jint)conv.unread_count,
        (jboolean)conv.is_pinned, (jboolean)conv.is_muted,
        (jlong)conv.updated_at_ms);

    env->DeleteLocalRef(convId);
    env->DeleteLocalRef(targetId);
    env->DeleteLocalRef(lastMsgId);
    env->DeleteLocalRef(lastMsgText);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertFriend(JNIEnv* env, const AnyChatFriend_C& friendInfo) {
    jclass cls = env->FindClass("com/anychat/sdk/models/Friend");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;JZLcom/anychat/sdk/models/UserInfo;)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring userId = toJString(env, friendInfo.user_id);
    jstring remark = toJString(env, friendInfo.remark);
    jobject userInfo = convertUserInfo(env, friendInfo.user_info);

    jobject obj = env->NewObject(cls, constructor,
        userId, remark, (jlong)friendInfo.updated_at_ms,
        (jboolean)friendInfo.is_deleted, userInfo);

    env->DeleteLocalRef(userId);
    env->DeleteLocalRef(remark);
    env->DeleteLocalRef(userInfo);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertFriendRequest(JNIEnv* env, const AnyChatFriendRequest_C& request) {
    jclass cls = env->FindClass("com/anychat/sdk/models/FriendRequest");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JLcom/anychat/sdk/models/UserInfo;)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring fromUserId = toJString(env, request.from_user_id);
    jstring toUserId = toJString(env, request.to_user_id);
    jstring message = toJString(env, request.message);
    jstring status = toJString(env, request.status);
    jobject fromUserInfo = convertUserInfo(env, request.from_user_info);

    jobject obj = env->NewObject(cls, constructor,
        (jlong)request.request_id, fromUserId, toUserId, message, status,
        (jlong)request.created_at_ms, fromUserInfo);

    env->DeleteLocalRef(fromUserId);
    env->DeleteLocalRef(toUserId);
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(status);
    env->DeleteLocalRef(fromUserInfo);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertGroup(JNIEnv* env, const AnyChatGroup_C& group) {
    jclass cls = env->FindClass("com/anychat/sdk/models/Group");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IIZJ)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring groupId = toJString(env, group.group_id);
    jstring name = toJString(env, group.name);
    jstring avatarUrl = toJString(env, group.avatar_url);
    jstring ownerId = toJString(env, group.owner_id);

    jobject obj = env->NewObject(cls, constructor,
        groupId, name, avatarUrl, ownerId, (jint)group.member_count,
        (jint)group.my_role, (jboolean)group.join_verify,
        (jlong)group.updated_at_ms);

    env->DeleteLocalRef(groupId);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(avatarUrl);
    env->DeleteLocalRef(ownerId);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertGroupMember(JNIEnv* env, const AnyChatGroupMember_C& member) {
    jclass cls = env->FindClass("com/anychat/sdk/models/GroupMember");
    if (!cls) return nullptr;

    jmethodID constructor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;IZJLcom/anychat/sdk/models/UserInfo;)V");
    if (!constructor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jstring userId = toJString(env, member.user_id);
    jstring groupNickname = toJString(env, member.group_nickname);
    jobject userInfo = convertUserInfo(env, member.user_info);

    jobject obj = env->NewObject(cls, constructor,
        userId, groupNickname, (jint)member.role, (jboolean)member.is_muted,
        (jlong)member.joined_at_ms, userInfo);

    env->DeleteLocalRef(userId);
    env->DeleteLocalRef(groupNickname);
    env->DeleteLocalRef(userInfo);
    env->DeleteLocalRef(cls);

    return obj;
}

jobject convertMessageList(JNIEnv* env, const AnyChatMessageList_C* list) {
    if (!list) return nullptr;

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject arrayList = env->NewObject(arrayListClass, arrayListInit, list->count);

    for (int i = 0; i < list->count; i++) {
        jobject msg = convertMessage(env, list->items[i]);
        if (msg) {
            env->CallBooleanMethod(arrayList, arrayListAdd, msg);
            env->DeleteLocalRef(msg);
        }
    }

    env->DeleteLocalRef(arrayListClass);
    return arrayList;
}

jobject convertConversationList(JNIEnv* env, const AnyChatConversationList_C* list) {
    if (!list) return nullptr;

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject arrayList = env->NewObject(arrayListClass, arrayListInit, list->count);

    for (int i = 0; i < list->count; i++) {
        jobject conv = convertConversation(env, list->items[i]);
        if (conv) {
            env->CallBooleanMethod(arrayList, arrayListAdd, conv);
            env->DeleteLocalRef(conv);
        }
    }

    env->DeleteLocalRef(arrayListClass);
    return arrayList;
}

jobject convertFriendList(JNIEnv* env, const AnyChatFriendList_C* list) {
    if (!list) return nullptr;

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject arrayList = env->NewObject(arrayListClass, arrayListInit, list->count);

    for (int i = 0; i < list->count; i++) {
        jobject friendObj = convertFriend(env, list->items[i]);
        if (friendObj) {
            env->CallBooleanMethod(arrayList, arrayListAdd, friendObj);
            env->DeleteLocalRef(friendObj);
        }
    }

    env->DeleteLocalRef(arrayListClass);
    return arrayList;
}

jobject convertGroupList(JNIEnv* env, const AnyChatGroupList_C* list) {
    if (!list) return nullptr;

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject arrayList = env->NewObject(arrayListClass, arrayListInit, list->count);

    for (int i = 0; i < list->count; i++) {
        jobject groupObj = convertGroup(env, list->items[i]);
        if (groupObj) {
            env->CallBooleanMethod(arrayList, arrayListAdd, groupObj);
            env->DeleteLocalRef(groupObj);
        }
    }

    env->DeleteLocalRef(arrayListClass);
    return arrayList;
}

jobject convertAuthDeviceList(JNIEnv* env, const AnyChatAuthDeviceList_C* list) {
    if (!list) return nullptr;

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID arrayListInit = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

    jobject arrayList = env->NewObject(arrayListClass, arrayListInit, list->count);

    for (int i = 0; i < list->count; i++) {
        jobject deviceObj = convertAuthDevice(env, list->items[i]);
        if (deviceObj) {
            env->CallBooleanMethod(arrayList, arrayListAdd, deviceObj);
            env->DeleteLocalRef(deviceObj);
        }
    }

    env->DeleteLocalRef(arrayListClass);
    return arrayList;
}

} // namespace jni
} // namespace anychat
