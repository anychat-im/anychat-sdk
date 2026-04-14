#pragma once

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes (0 = success) */
#define ANYCHAT_OK 0
#define ANYCHAT_ERROR_INVALID_PARAM 1
#define ANYCHAT_ERROR_AUTH 2
#define ANYCHAT_ERROR_NETWORK 3
#define ANYCHAT_ERROR_TIMEOUT 4
#define ANYCHAT_ERROR_NOT_FOUND 5
#define ANYCHAT_ERROR_ALREADY_EXISTS 6
#define ANYCHAT_ERROR_INTERNAL 7
#define ANYCHAT_ERROR_NOT_LOGGED_IN 8
#define ANYCHAT_ERROR_TOKEN_EXPIRED 9

#ifdef __cplusplus
}
#endif
