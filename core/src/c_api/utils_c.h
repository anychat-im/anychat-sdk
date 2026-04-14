#pragma once

/* Internal helpers shared across c_api implementation files.
 * Not part of the public API. */

#include <cstddef>
#include <string>

/* Duplicate a C string with malloc. Returns nullptr if s is nullptr. */
char* anychat_strdup(const char* s);

/* Safe string copy: always NUL-terminates, truncates if necessary. */
void anychat_strlcpy(char* dst, const char* src, size_t dst_size);

/* Set / clear the thread-local last-error message. */
void anychat_set_last_error(const std::string& error);
void anychat_clear_last_error();
