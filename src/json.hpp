#pragma once
#include "cJSON.h"
#include "alerts/time.hpp"
#include <cctype>
#include <memory>
#include <string>
#include <string_view>

namespace alerts::json {
using Document = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
inline Document parse(std::string_view text) {
    if (text.empty() || text.size() > 262144) return {nullptr, cJSON_Delete};
    const char* end = nullptr;
    Document doc(cJSON_ParseWithLengthOpts(text.data(), text.size(), &end, false), cJSON_Delete);
    if (!doc) return doc;
    const char* limit = text.data() + text.size();
    while (end < limit && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end != limit) return {nullptr, cJSON_Delete};
    return doc;
}
inline const cJSON* field(const cJSON* value, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(value, key);
}
inline std::string text(const cJSON* value) {
    return cJSON_IsString(value) ? value->valuestring : "";
}
inline std::string text(const cJSON* value, const char* key) {
    return text(field(value, key));
}
inline bool timestamp(const std::string& value) {
    return parse_utc(value).has_value();
}
}  // namespace alerts::json
