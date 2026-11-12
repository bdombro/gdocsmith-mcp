#pragma once

#include "json.hpp"
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace gdocsmith::lens::json {

inline bool is_null(const slimr::Json& j) { return j.is_null(); }
inline bool is_bool(const slimr::Json& j) { return j.is_boolean(); }
inline bool is_number(const slimr::Json& j) { return j.is_number(); }
inline bool is_string(const slimr::Json& j) { return j.is_string(); }
inline bool is_array(const slimr::Json& j) { return j.is_array(); }
inline bool is_object(const slimr::Json& j) { return j.is_object(); }

inline double as_number(const slimr::Json& j, double def = 0.0) {
    if (j.is_number()) return j.as<double>();
    return def;
}

inline int64_t as_int64(const slimr::Json& j, int64_t def = 0) {
    if (j.is_number()) return j.as<int64_t>();
    return def;
}

inline const std::string& as_string(const slimr::Json& j) {
    static const std::string empty;
    if (j.is_string()) return j.get<std::string>();
    return empty;
}

inline bool as_bool(const slimr::Json& j, bool def = false) {
    if (j.is_boolean()) return j.get<bool>();
    return def;
}

inline const slimr::Json::object_t* as_object_ptr(const slimr::Json& j) {
    return j.get_if<slimr::Json::object_t>();
}

inline const slimr::Json::array_t* as_array_ptr(const slimr::Json& j) {
    return j.get_if<slimr::Json::array_t>();
}

inline const slimr::Json* get(const slimr::Json& j, std::string_view key) {
    if (auto* obj = j.get_if<slimr::Json::object_t>()) {
        auto it = obj->find(key);
        if (it != obj->end()) return &it->second;
    }
    return nullptr;
}

inline slimr::Json* get_mut(slimr::Json& j, std::string_view key) {
    if (auto* obj = j.get_if<slimr::Json::object_t>()) {
        auto it = obj->find(key);
        if (it != obj->end()) return &it->second;
    }
    return nullptr;
}

inline void set(slimr::Json& j, std::string_view key, slimr::Json val) {
    j[key] = std::move(val);
}

inline void push_back(slimr::Json& j, slimr::Json val) {
    if (j.is_null()) j.data = slimr::Json::array_t{};
    if (auto* arr = j.get_if<slimr::Json::array_t>()) {
        arr->push_back(std::move(val));
    }
}

inline slimr::Json make_object() {
    return slimr::json_obj();
}

inline slimr::Json make_array() {
    return slimr::json_arr();
}

} // namespace gdocsmith::lens::json
