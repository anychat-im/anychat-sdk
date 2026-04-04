/*
 * anychat_c.h — AnyChat SDK C interface
 *
 * This is the single include entry point. Include only this file.
 *
 * All strings are UTF-8. All callbacks fire on an internal SDK thread;
 * do not call SDK functions from within a callback unless documented as
 * reentrant-safe. All SDK-allocated strings and lists must be freed with
 * the corresponding anychat_free_* function.
 *
 * Memory ownership:
 *   - String *inputs*  : caller-owned; SDK copies internally.
 *   - String *outputs* : SDK-allocated; caller frees with anychat_free_string().
 *   - List outputs     : SDK-allocated; caller frees with the matching
 *                        anychat_free_*_list() function.
 *   - Sub-module handles returned by anychat_client_get_*() are owned by the
 *     client and must NOT be freed individually.
 */
#pragma once

#include "auth_c.h"
#include "client_c.h"
#include "conversation_c.h"
#include "errors_c.h"
#include "file_c.h"
#include "friend_c.h"
#include "group_c.h"
#include "message_c.h"
#include "call_c.h"
#include "types_c.h"
#include "user_c.h"
