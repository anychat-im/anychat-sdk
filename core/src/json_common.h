#pragma once

#include <glaze/glaze.hpp>

#include "network/http_client.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <utility>
#include <variant>

namespace anychat::json_common {

template <typename T>
struct ApiEnvelope {
    int32_t code = -1;
    std::string message{};
    T data{};
};

struct ApiStatus {
    int32_t code = -1;
    std::string message{};
};

struct TimestampObject {
    int64_t seconds = 0;
    int64_t nanos = 0;
};

using TimestampValue = std::variant<int64_t, double, std::string, TimestampObject>;
using OptionalTimestampValue = std::optional<TimestampValue>;

inline int64_t normalizeUnixEpochMs(int64_t raw) {
    // Server payloads usually use Unix seconds. Keep millisecond inputs as-is.
    return (raw > 0 && raw < 1'000'000'000'000LL) ? raw * 1000LL : raw;
}

inline std::string formatIso8601Utc(int64_t unix_ms) {
    if (unix_ms <= 0) {
        return "";
    }

    const std::time_t unix_seconds = static_cast<std::time_t>(unix_ms / 1000LL);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &unix_seconds);
#else
    gmtime_r(&unix_seconds, &tm_utc);
#endif

    char buffer[32] = { 0 };
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        return "";
    }
    return std::string(buffer);
}

inline time_t utcTimeFromTm(std::tm* tm_utc) {
#if defined(_WIN32)
    return _mkgmtime(tm_utc);
#else
    return timegm(tm_utc);
#endif
}

inline int parseTwoDigits(const std::string& text, size_t pos) {
    if (pos + 2 > text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))
        || !std::isdigit(static_cast<unsigned char>(text[pos + 1]))) {
        return -1;
    }
    return (text[pos] - '0') * 10 + (text[pos + 1] - '0');
}

inline int64_t parseTimestampMs(const TimestampValue& value) {
    if (const auto* int_value = std::get_if<int64_t>(&value); int_value != nullptr) {
        return normalizeUnixEpochMs(*int_value);
    }
    if (const auto* dbl_value = std::get_if<double>(&value); dbl_value != nullptr) {
        return normalizeUnixEpochMs(static_cast<int64_t>(*dbl_value));
    }
    if (const auto* obj_value = std::get_if<TimestampObject>(&value); obj_value != nullptr) {
        if (obj_value->seconds <= 0 && obj_value->nanos <= 0) {
            return 0;
        }
        return obj_value->seconds * 1000 + obj_value->nanos / 1000000;
    }

    const std::string& text = std::get<std::string>(value);
    if (text.empty()) {
        return 0;
    }

    const bool all_digits = std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
    if (all_digits) {
        try {
            return normalizeUnixEpochMs(std::stoll(text));
        } catch (...) {
            return 0;
        }
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6
        && std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }

    std::tm tm_utc{};
    tm_utc.tm_year = year - 1900;
    tm_utc.tm_mon = month - 1;
    tm_utc.tm_mday = day;
    tm_utc.tm_hour = hour;
    tm_utc.tm_min = minute;
    tm_utc.tm_sec = second;

    time_t epoch_seconds = utcTimeFromTm(&tm_utc);
    if (epoch_seconds == static_cast<time_t>(-1)) {
        return 0;
    }

    int ms = 0;
    const size_t dot_pos = text.find('.', 19);
    if (dot_pos != std::string::npos) {
        int scale = 100;
        for (size_t i = dot_pos + 1; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) && scale > 0;
             ++i) {
            ms += (text[i] - '0') * scale;
            scale /= 10;
        }
    }

    int tz_offset_seconds = 0;
    const size_t tz_pos = text.find_first_of("+-Z", 19);
    if (tz_pos != std::string::npos && text[tz_pos] != 'Z') {
        const int hh = parseTwoDigits(text, tz_pos + 1);
        if (hh >= 0) {
            int mm = 0;
            if (tz_pos + 3 < text.size() && text[tz_pos + 3] == ':') {
                const int parsed = parseTwoDigits(text, tz_pos + 4);
                if (parsed >= 0) {
                    mm = parsed;
                }
            } else if (tz_pos + 3 < text.size() && std::isdigit(static_cast<unsigned char>(text[tz_pos + 3]))) {
                const int parsed = parseTwoDigits(text, tz_pos + 3);
                if (parsed >= 0) {
                    mm = parsed;
                }
            }

            tz_offset_seconds = hh * 3600 + mm * 60;
            if (text[tz_pos] == '-') {
                tz_offset_seconds = -tz_offset_seconds;
            }
        }
    }

    return static_cast<int64_t>(epoch_seconds - tz_offset_seconds) * 1000LL + ms;
}

inline int64_t parseTimestampMs(const OptionalTimestampValue& value) {
    if (!value.has_value()) {
        return 0;
    }
    return parseTimestampMs(*value);
}

inline int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

inline std::string toLowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

inline std::string toLower(std::string text) {
    return toLowerCopy(std::move(text));
}

template <typename T>
struct IsStdVariant : std::false_type {};

template <typename... Ts>
struct IsStdVariant<std::variant<Ts...>> : std::true_type {};

template <typename T>
inline constexpr bool kIsStdVariant = IsStdVariant<std::remove_cvref_t<T>>::value;

template <typename T>
struct IsOptional : std::false_type {};

template <typename U>
struct IsOptional<std::optional<U>> : std::true_type {};

template <typename T>
inline constexpr bool kIsOptional = IsOptional<std::remove_cvref_t<T>>::value;

template <typename T, typename VariantT>
struct VariantHasType : std::false_type {};

template <typename T, typename... Ts>
struct VariantHasType<T, std::variant<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <typename T, typename VariantT>
inline constexpr bool kVariantHasType = VariantHasType<T, std::remove_cvref_t<VariantT>>::value;

template <typename T, typename VariantT>
inline const T* getVariantIf(const VariantT& value) {
    if constexpr (kIsStdVariant<VariantT> && kVariantHasType<T, VariantT>) {
        return std::get_if<T>(&value);
    } else {
        return nullptr;
    }
}

inline int64_t parseInt64String(const std::string& text, int64_t def) {
    if (text.empty()) {
        return def;
    }

    char* end = nullptr;
    const int64_t parsed = std::strtoll(text.c_str(), &end, 10);
    return (end != nullptr && *end == '\0') ? parsed : def;
}

inline bool parseBoolString(const std::string& text, bool& value_out) {
    const std::string lowered = toLowerCopy(text);
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on") {
        value_out = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off") {
        value_out = false;
        return true;
    }
    return false;
}

template <typename ValueT>
    requires (!kIsOptional<ValueT>)
inline int64_t parseInt64Value(const ValueT& value, int64_t def = 0) {
    using RawT = std::remove_cvref_t<ValueT>;

    if constexpr (std::is_same_v<RawT, bool>) {
        return value ? 1 : 0;
    }
    if constexpr (std::is_integral_v<RawT> && !std::is_same_v<RawT, bool>) {
        return static_cast<int64_t>(value);
    }
    if constexpr (std::is_floating_point_v<RawT>) {
        return static_cast<int64_t>(value);
    }
    if constexpr (std::is_same_v<RawT, std::string>) {
        return parseInt64String(value, def);
    }

    if (const auto* int_value = getVariantIf<int64_t>(value); int_value != nullptr) {
        return *int_value;
    }
    if (const auto* double_value = getVariantIf<double>(value); double_value != nullptr) {
        return static_cast<int64_t>(*double_value);
    }
    if (const auto* bool_value = getVariantIf<bool>(value); bool_value != nullptr) {
        return *bool_value ? 1 : 0;
    }
    if (const auto* text = getVariantIf<std::string>(value); text != nullptr) {
        return parseInt64String(*text, def);
    }

    return def;
}

template <typename VariantT>
inline int64_t parseInt64Value(const std::optional<VariantT>& value, int64_t def = 0) {
    return value.has_value() ? parseInt64Value(*value, def) : def;
}

template <typename VariantT>
inline int32_t parseInt32Value(const std::optional<VariantT>& value, int32_t def = 0) {
    return static_cast<int32_t>(parseInt64Value(value, def));
}

template <typename ValueT>
    requires (!kIsOptional<ValueT>)
inline bool parseBoolValue(const ValueT& value, bool def = false) {
    using RawT = std::remove_cvref_t<ValueT>;

    if constexpr (std::is_same_v<RawT, bool>) {
        return value;
    }
    if constexpr (std::is_integral_v<RawT> && !std::is_same_v<RawT, bool>) {
        return value != 0;
    }
    if constexpr (std::is_floating_point_v<RawT>) {
        return value != 0.0;
    }
    if constexpr (std::is_same_v<RawT, std::string>) {
        bool parsed = false;
        return parseBoolString(value, parsed) ? parsed : def;
    }

    if (const auto* bool_value = getVariantIf<bool>(value); bool_value != nullptr) {
        return *bool_value;
    }
    if (const auto* int_value = getVariantIf<int64_t>(value); int_value != nullptr) {
        return *int_value != 0;
    }
    if (const auto* double_value = getVariantIf<double>(value); double_value != nullptr) {
        return *double_value != 0.0;
    }
    if (const auto* text = getVariantIf<std::string>(value); text != nullptr) {
        bool parsed = false;
        return parseBoolString(*text, parsed) ? parsed : def;
    }

    return def;
}

template <typename VariantT>
inline bool parseBoolValue(const std::optional<VariantT>& value, bool def = false) {
    return value.has_value() ? parseBoolValue(*value, def) : def;
}

template <typename T, typename... Rest>
inline const std::vector<T>* pickList(const std::optional<std::vector<T>>& first, const Rest&... rest) {
    if (first.has_value()) {
        return &(*first);
    }
    if constexpr (sizeof...(rest) == 0) {
        return nullptr;
    } else {
        return pickList<T>(rest...);
    }
}

template <typename T>
inline bool readJsonRelaxed(const std::string& json, T& out, std::string& err) {
    glz::context ctx{};
    const auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(out, json, ctx);
    if (ec) {
        err = std::string("parse error: ") + glz::format_error(ec, json);
        return false;
    }
    return true;
}

template <typename T>
inline bool parseApiEnvelopeResponse(
    const network::HttpResponse& resp,
    ApiEnvelope<T>& root,
    std::string& err,
    const std::string& fallback_error = "server error",
    bool require_http_200 = false,
    bool require_http_2xx = false
) {
    if (!resp.error.empty()) {
        err = resp.error;
        return false;
    }

    if (require_http_200 && resp.status_code != 200) {
        err = "HTTP " + std::to_string(resp.status_code);
        return false;
    }
    if (require_http_2xx && (resp.status_code < 200 || resp.status_code >= 300)) {
        err = "HTTP " + std::to_string(resp.status_code);
        return false;
    }

    if (!readJsonRelaxed(resp.body, root, err)) {
        return false;
    }

    if (root.code != 0) {
        err = root.message.empty() ? fallback_error : root.message;
        return false;
    }

    return true;
}

inline bool parseApiStatusResponse(
    const network::HttpResponse& resp,
    ApiStatus& root,
    std::string& err,
    bool require_http_200 = false,
    bool require_http_2xx = false
) {
    if (!resp.error.empty()) {
        err = resp.error;
        return false;
    }

    if (require_http_200 && resp.status_code != 200) {
        err = "HTTP " + std::to_string(resp.status_code);
        return false;
    }
    if (require_http_2xx && (resp.status_code < 200 || resp.status_code >= 300)) {
        err = "HTTP " + std::to_string(resp.status_code);
        return false;
    }

    return readJsonRelaxed(resp.body, root, err);
}

inline bool parseApiStatusSuccessResponse(
    const network::HttpResponse& resp,
    std::string& err,
    const std::string& fallback_error = "server error",
    bool require_http_200 = false,
    bool require_http_2xx = false
) {
    ApiStatus root{};
    if (!parseApiStatusResponse(resp, root, err, require_http_200, require_http_2xx)) {
        return false;
    }

    if (root.code != 0) {
        err = root.message.empty() ? fallback_error : root.message;
        return false;
    }
    return true;
}

template <typename T>
inline bool writeJson(const T& value, std::string& json, std::string& err) {
    const auto written = glz::write_json(value);
    if (!written) {
        err = std::string("serialize error: ") + glz::format_error(written);
        return false;
    }
    json = *written;
    return true;
}

template <typename T>
inline std::optional<T> decodeJsonObject(const std::string& node_json, std::string& err) {
    if (node_json.empty()) {
        err = "parse error: empty object";
        return std::nullopt;
    }

    T out{};
    if (!readJsonRelaxed(node_json, out, err)) {
        return std::nullopt;
    }
    return out;
}

template <typename T>
inline bool parseJsonObject(const std::string& node_json, T& out, std::string& err) {
    auto parsed = decodeJsonObject<T>(node_json, err);
    if (!parsed.has_value()) {
        return false;
    }
    out = std::move(*parsed);
    return true;
}

} // namespace anychat::json_common
