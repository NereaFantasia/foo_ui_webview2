#pragma once
// 已声明方法的返回值：生成的 ToJson（src/api/generated/*Schema.h）共用的写入工具，与 ApiParams.h
// 对称；以及 handler 的返回类型 Result<R> 和失败信封。
//
// 本头只依赖 nlohmann/json、标准库与 ErrorEnvelope.h，不引 pch.h 或 foobar2000 SDK。ErrorEnvelope.h
// 里的 FailureHook 调 console::printf，测试工程包含本头（或包含它的生成头）之前要先包含
// compat/fb2k_types.h 的桩。
// 声明里可选的字段（`x?: T`）省略时不写键；可空的字段（`x: T | null`）省略时写 null。
// 两者在 C++ 侧都是 std::optional<T>，由生成器按声明选择 Put 还是 PutNullable。
#include <nlohmann/json.hpp>

#include "api/ErrorEnvelope.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace api::results {

using json = nlohmann::json;

// ---- 单值转换 ----
//
// 模板重载先全部声明再定义：vector<optional<结构体>> 这类组合在实例化时只能看到定义点之前
// 声明的重载，ADL 也不会查到本命名空间（与 ApiParams.h 的 Convert 同一处理）。

template <class T>
json Value(const std::optional<T>& v);
template <class T>
json Value(const std::vector<T>& v);
template <class T>
json Value(const std::map<std::string, T>& v);

// 生成的结构体各自提供 ToJson(const T&)，这里经 ADL 转发，让 vector<结构体>、map<string, 结构体> 也能走同一套 Value。
// 声明处直接定义、不另写前置声明：v143（MSVC 14.44）认不出尾置 decltype 的前置声明与后面的定义是同一个模板，
// 调用处报 C2668 歧义。
template <class T>
auto Value(const T& v) -> decltype(ToJson(v)) {
    return ToJson(v);
}

inline json Value(const std::string& v) { return v; }
// 字符串字面量要有这个重载，否则经数组→指针→bool 的标准转换选中 Value(bool)，静默写成 true。
// 它必须在下面的 Put 模板之前声明：Put 体内的 Value(v) 是依赖调用，const char[N] 没有关联命名空间，
// 实例化时只看得见定义点之前的重载。
inline json Value(const char* v) { return v; }
inline json Value(std::int64_t v) { return v; }
// 声明为 Json 的字段：原样写出。
inline json Value(const json& v) { return v; }
inline json Value(double v) { return v; }
inline json Value(bool v) { return v; }

template <class T>
json Value(const std::optional<T>& v) {
    return v ? Value(*v) : json(nullptr);
}

template <class T>
json Value(const std::vector<T>& v) {
    json a = json::array();
    for (const T& item : v) a.push_back(Value(item));
    return a;
}

template <class T>
json Value(const std::map<std::string, T>& v) {
    json o = json::object();
    for (const auto& [key, item] : v) o[key] = Value(item);
    return o;
}

// ---- 对象级写入 ----

template <class T>
void Put(json& j, const char* key, const T& v) {
    j[key] = Value(v);
}

// 可选字段：没有值就不写键。
template <class T>
void Put(json& j, const char* key, const std::optional<T>& v) {
    if (v) j[key] = Value(*v);
}

// 可空字段：没有值写 null。
template <class T>
void PutNullable(json& j, const char* key, const std::optional<T>& v) {
    j[key] = v ? Value(*v) : json(nullptr);
}

// `A & Record<string, T>` 的额外键。生成的 ToJson 先写它们再写具名字段，同名时具名字段覆盖。
template <class T>
void PutAll(json& j, const std::map<std::string, T>& extra) {
    for (const auto& [key, item] : extra) j[key] = Value(item);
}

}  // namespace api::results

namespace api {

// handler 的业务失败。code 取 ApiErrorCode:: 常量，按值持有，handler 传局部字符串也不会悬垂。
// extra 放失败信封里要保留的既有字段：details、迁移前就在失败里的字段（如 window 失败带的窗口状态）；
// 与 success / error / code 同名的键不生效。用 object_t 而不是 json：少写一层花括号的
// {"path", path} 会被 json 收成数组，object_t 则直接编译失败。
struct Failure {
    std::string error;
    std::string code;
    nlohmann::json::object_t extra;
};

inline Failure Fail(std::string error, std::string code, nlohmann::json::object_t extra = {}) {
    return Failure{std::move(error), std::move(code), std::move(extra)};
}

// 从 DUI/CUI 面板调用只在独立主窗口里有意义的方法。页面要预先判断运行模式时用 window.getMode。
inline Failure PanelModeUnsupported(std::string_view api) {
    return Fail(std::string(api) + " is not supported in panel mode", ApiErrorCode::PANEL_MODE_UNSUPPORTED);
}

namespace results {

// 失败信封只有 ApiEnvelope::MakeError 一处定义；extra 只补信封里没有的键。
inline json FailureToJson(const Failure& f) {
    json j = ApiEnvelope::MakeError(f.error, f.code.c_str());
    for (const auto& [key, value] : f.extra) {
        if (!j.contains(key)) j[key] = value;
    }
    return j;
}

}  // namespace results

// handler 的返回类型：恰好持有一个成功值或一个 Failure，由注册包装层调 ToJson 序列化。
// 没有默认构造，handler 写 return {}; 编译不过，不会被误当成成功或失败。
template <class R>
class Result {
public:
    Result(R value) : state_(std::in_place_index<0>, std::move(value)) {}
    Result(Failure failure) : state_(std::in_place_index<1>, std::move(failure)) {}

    // 成功时 success 最后写，声明的具名字段与 A & Record 的额外键都盖不掉它。转发经 results::Value：
    // 成员自己叫 ToJson，成员内的非限定 ToJson(value) 会先找到成员而跳过 ADL。
    nlohmann::json ToJson() const {
        if (const R* value = std::get_if<0>(&state_)) {
            nlohmann::json j = results::Value(*value);
            j["success"] = true;
            return j;
        }
        return results::FailureToJson(std::get<1>(state_));
    }

private:
    std::variant<R, Failure> state_;
};

template <>
class Result<void>;
inline Result<void> Ok();

// 声明返回 void 的方法：成功只回 {success:true}，用 Ok() 构造。
template <>
class Result<void> {
public:
    Result(Failure failure) : failure_(std::move(failure)) {}

    nlohmann::json ToJson() const {
        return failure_ ? results::FailureToJson(*failure_) : nlohmann::json{{"success", true}};
    }

private:
    struct OkTag {};
    explicit Result(OkTag) {}
    friend Result<void> Ok();

    std::optional<Failure> failure_;
};

inline Result<void> Ok() { return Result<void>(Result<void>::OkTag{}); }

}  // namespace api
