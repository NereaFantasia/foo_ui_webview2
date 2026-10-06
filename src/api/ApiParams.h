#pragma once
// 生成的参数解析代码（src/api/generated/*Schema.h）共用的读取工具。
//
// 本头只依赖 nlohmann/json 与标准库，不引 pch.h 或 foobar2000 SDK，测试工程可以直接包含。
// 解析失败时只报第一处错误，消息原样放进 INVALID_PARAMS 的 error 字段，所以措辞面向调用方：
// 缺字段与空字符串都报 "<key> is required"，与迁移前各 handler 的手写消息保持一致。
#include <nlohmann/json.hpp>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace api::params {

using json = nlohmann::json;

// 路径参数要走的安全校验层级。取值与 BridgeCore.h 的 SecurityLevel 一一对应，
// TypedApi.h 用 static_assert 保证两边不会错位；这里单独定义是为了不把 BridgeCore.h 带进测试工程。
enum class PathAccess { Read = 1, Write = 2, MediaRead = 3, MediaWrite = 4, FileWrite = 5 };

struct PathParam {
    const char* key;
    PathAccess access;
    bool isArray;
    // Arrays only: drop the paths that fail the check instead of refusing the call; the
    // bridge then adds `skippedPaths` to the success response.
    bool skipInvalid;
    // Arrays of objects only: the member of every element that holds the path
    // (items[].path); nullptr for a string or an array of strings.
    const char* nestedKey;
    // With nestedKey: an element may also be the path string itself (handles[] that take
    // "path" or { path, subsong }). Elements that are neither carry no path and are not checked.
    bool stringElements = false;
};

// 调用方看到的字段路径：顶层是键名，嵌套是 a.b，数组元素是 a[3]。
inline std::string Join(const std::string& where, std::string_view key) {
    return where.empty() ? std::string(key) : where + "." + std::string(key);
}

// ---- 单值转换：类型不符时写 error 并返回 false ----
//
// 模板重载先全部声明再定义：vector<map<…>> 这类组合在实例化时只能看到定义点之前声明的重载，
// ADL 也不会查到本命名空间。

template <class T>
bool Convert(const json& v, std::vector<T>& out, const std::string& where, std::string& error);
template <class T>
bool Convert(const json& v, std::map<std::string, T>& out, const std::string& where, std::string& error);

// 生成的嵌套结构体各自提供 FromJson(const json&, T&, std::string&, const std::string&)，
// 这里经 ADL 转发，让 vector<结构体>、map<string, 结构体> 也能走同一套 Convert。
// 声明处直接定义、不另写前置声明：v143（MSVC 14.44）认不出尾置 decltype 的前置声明与后面的定义是同一个模板，
// 调用处报 C2668 歧义。
template <class T>
auto Convert(const json& v, T& out, const std::string& where, std::string& error)
    -> decltype(FromJson(v, out, error, where)) {
    return FromJson(v, out, error, where);
}


// 声明为 Json 的值：任意 JSON，原样保存，不做类型检查。
inline bool Convert(const json& v, json& out, const std::string& /*where*/, std::string& /*error*/) {
    out = v;
    return true;
}

inline bool Convert(const json& v, std::string& out, const std::string& where, std::string& error) {
    if (!v.is_string()) { error = where + " must be a string"; return false; }
    out = v.get<std::string>();
    return true;
}

inline bool Convert(const json& v, std::int64_t& out, const std::string& where, std::string& error) {
    // is_number_integer 同时覆盖有符号与无符号；超出 int64 的无符号数按越界处理。
    if (!v.is_number_integer()) { error = where + " must be an integer"; return false; }
    if (v.is_number_unsigned() && v.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        error = where + " is out of range";
        return false;
    }
    out = v.get<std::int64_t>();
    return true;
}

inline bool Convert(const json& v, double& out, const std::string& where, std::string& error) {
    if (!v.is_number()) { error = where + " must be a number"; return false; }
    out = v.get<double>();
    return true;
}

inline bool Convert(const json& v, bool& out, const std::string& where, std::string& error) {
    if (!v.is_boolean()) { error = where + " must be a boolean"; return false; }
    out = v.get<bool>();
    return true;
}

template <class T>
bool Convert(const json& v, std::vector<T>& out, const std::string& where, std::string& error) {
    if (!v.is_array()) { error = where + " must be an array"; return false; }
    out.clear();
    out.reserve(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        T item{};
        if (!Convert(v[i], item, where + "[" + std::to_string(i) + "]", error)) return false;
        out.push_back(std::move(item));
    }
    return true;
}

template <class T>
bool Convert(const json& v, std::map<std::string, T>& out, const std::string& where, std::string& error) {
    if (!v.is_object()) { error = where + " must be an object"; return false; }
    out.clear();
    for (auto it = v.begin(); it != v.end(); ++it) {
        T item{};
        if (!Convert(it.value(), item, Join(where, it.key()), error)) return false;
        out.emplace(it.key(), std::move(item));
    }
    return true;
}

// ---- 对象级读取 ----

class Reader {
public:
    Reader(const json& obj, std::string& error, const std::string& where = {})
        : obj_(obj), error_(error), where_(where) {}
    // Reader 只保存引用，禁止绑定临时对象，否则构造语句结束时就悬空。
    Reader(const json&& obj, std::string& error, const std::string& where = {}) = delete;

    bool IsObject() {
        if (obj_.is_object()) return true;
        error_ = where_.empty() ? "params must be an object" : where_ + " must be an object";
        return false;
    }

    // 以下划线开头的键留给桥接层注入（如 BridgeCore 写入的 _callerHwnd），不算未知键。
    bool OnlyKeys(std::initializer_list<std::string_view> known) {
        for (auto it = obj_.begin(); it != obj_.end(); ++it) {
            const std::string& key = it.key();
            if (!key.empty() && key.front() == '_') continue;
            bool found = false;
            for (auto k : known) {
                if (k == key) { found = true; break; }
            }
            if (!found) {
                error_ = "unknown parameter '" + Join(where_, key) + "'";
                return false;
            }
        }
        return true;
    }

    template <class T>
    bool Required(const char* key, T& out) {
        auto it = obj_.find(key);
        if (it == obj_.end() || it->is_null()) {
            error_ = Join(where_, key) + " is required";
            return false;
        }
        return Convert(*it, out, Join(where_, key), error_);
    }

    // 缺省时保留 out 里的默认值（由生成的成员初始化器给出）。
    template <class T>
    bool Optional(const char* key, T& out) {
        auto it = obj_.find(key);
        if (it == obj_.end() || it->is_null()) return true;
        return Convert(*it, out, Join(where_, key), error_);
    }

    template <class T>
    bool Optional(const char* key, std::optional<T>& out) {
        auto it = obj_.find(key);
        if (it == obj_.end() || it->is_null()) { out.reset(); return true; }
        T value{};
        if (!Convert(*it, value, Join(where_, key), error_)) return false;
        out = std::move(value);
        return true;
    }

    // minLength 为 1 时空串按缺失处理，沿用迁移前 "path is required" 这类消息。
    bool MinLength(const char* key, const std::string& value, std::size_t min) {
        if (value.size() >= min) return true;
        error_ = min == 1 ? Join(where_, key) + " is required"
                          : Join(where_, key) + " must be at least " + std::to_string(min) + " characters";
        return false;
    }

    bool MinLength(const char* key, const std::optional<std::string>& value, std::size_t min) {
        return !value || MinLength(key, *value, min);
    }

    // min / max 不参与推导，生成代码可以直接传 std::nullopt。
    template <class N>
    bool Range(const char* key, N value, std::type_identity_t<std::optional<N>> min,
               std::type_identity_t<std::optional<N>> max) {
        if ((min && value < *min) || (max && value > *max)) {
            error_ = Join(where_, key) + " is out of range";
            return false;
        }
        return true;
    }

    template <class N>
    bool Range(const char* key, const std::optional<N>& value, std::type_identity_t<std::optional<N>> min,
               std::type_identity_t<std::optional<N>> max) {
        return !value || Range(key, *value, min, max);
    }

    // 数组的每一项都要落在范围内；报错时带上出错的下标。
    template <class N>
    bool ItemsRange(const char* key, const std::vector<N>& values, std::type_identity_t<std::optional<N>> min,
                    std::type_identity_t<std::optional<N>> max) {
        for (std::size_t i = 0; i < values.size(); ++i) {
            if ((min && values[i] < *min) || (max && values[i] > *max)) {
                error_ = Join(where_, key) + "[" + std::to_string(i) + "] is out of range";
                return false;
            }
        }
        return true;
    }

    template <class N>
    bool ItemsRange(const char* key, const std::optional<std::vector<N>>& values,
                    std::type_identity_t<std::optional<N>> min, std::type_identity_t<std::optional<N>> max) {
        return !values || ItemsRange(key, *values, min, max);
    }

    bool OneOf(const char* key, const std::string& value, std::initializer_list<std::string_view> allowed) {
        for (auto a : allowed) {
            if (a == value) return true;
        }
        error_ = Join(where_, key) + " has an unsupported value '" + value + "'";
        return false;
    }

    bool OneOf(const char* key, const std::optional<std::string>& value,
               std::initializer_list<std::string_view> allowed) {
        return !value || OneOf(key, *value, allowed);
    }

    template <class T>
    bool MinItems(const char* key, const std::vector<T>& value, std::size_t min) {
        if (value.size() >= min) return true;
        error_ = Join(where_, key) + " must have at least " + std::to_string(min) + " item(s)";
        return false;
    }

    template <class T>
    bool MinItems(const char* key, const std::optional<std::vector<T>>& value, std::size_t min) {
        return !value || MinItems(key, *value, min);
    }

private:
    const json& obj_;
    std::string& error_;
    std::string where_;
};

}  // namespace api::params
