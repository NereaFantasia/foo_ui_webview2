#pragma once

// ============================================
// PreferencesDraft.h - Preferences 页面草稿与提交的纯逻辑
//
// 每个偏好设置页只维护一份“初始快照”与一份“编辑副本”。本头文件承担与 HWND、
// cfg_var 和 foobar2000 SDK 都无关的部分：字段表、差异、脏判定、默认值、校验、
// 外部写入冲突判定、重启判定，让 GoogleTest 直接覆盖真实生产符号。
//
// 字段表（FieldTable）由各页声明：每个字段给出键、类型、默认值、枚举上限、
// 是否影响重启，以及可选的额外校验；快照（Snapshot）是与字段表同序的一列取值。
// 页面之间只共享这套机制，不共享任何具体字段。
//
// 语义：
//   - dirty 按差异计算，改回原值即清除；
//   - Reset 只把副本设为默认值，不写配置；
//   - Apply 前重新读配置：只有已编辑字段参与冲突判定，外部把某个已编辑字段
//     改成与基线及草稿都不同的值才算冲突；未编辑字段保留当前值，不被旧快照覆盖；
//   - 影响重启的字段与本进程启动值不同即报告需要重启，Applied 之后也保留。
// ============================================

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

namespace prefs_draft {

enum class Kind {
    Bool,
    Int,
    String,
};

// 一个字段的取值。三种类型共用一个结构，未使用的成员保持默认值，比较时只看 kind 对应的成员。
struct Value {
    Kind kind = Kind::Bool;
    bool b = false;
    int i = 0;
    std::string s;

    static Value Bool(bool value) { Value v; v.kind = Kind::Bool; v.b = value; return v; }
    static Value Int(int value) { Value v; v.kind = Kind::Int; v.i = value; return v; }
    static Value Str(std::string value) { Value v; v.kind = Kind::String; v.s = std::move(value); return v; }

    bool operator==(const Value& other) const {
        if (kind != other.kind) return false;
        switch (kind) {
        case Kind::Bool: return b == other.b;
        case Kind::Int: return i == other.i;
        case Kind::String: return s == other.s;
        }
        return false;
    }
    bool operator!=(const Value& other) const { return !(*this == other); }
};

enum class ValidationError {
    None,
    OutOfRange,   // Int 字段不在 [0, enumCount)
    Invalid,      // 额外校验判为格式不对（例如模板名含非法字符）
    Missing,      // 额外校验判为所指对象不存在（例如模板目录或 index.html 缺失）
};

struct FieldSpec {
    std::string key;                  // 稳定标识，用于日志与测试；不进 UI
    Kind kind = Kind::Bool;
    Value defaultValue;
    int enumCount = 0;                // Int 字段大于 0 时按 [0, enumCount) 校验
    bool affectsRestart = false;      // 与启动值不同即要求重启
    // 额外校验，在枚举范围校验之后运行；返回 None 表示通过。
    std::function<ValidationError(const Value&)> validate;
};

using FieldTable = std::vector<FieldSpec>;

// 与字段表同序的一列取值。
struct Snapshot {
    std::vector<Value> values;

    size_t size() const { return values.size(); }
    const Value& at(size_t field) const { return values.at(field); }
    Value& at(size_t field) { return values.at(field); }

    bool GetBool(size_t field) const { return values.at(field).b; }
    int GetInt(size_t field) const { return values.at(field).i; }
    const std::string& GetString(size_t field) const { return values.at(field).s; }

    void SetBool(size_t field, bool value) { values.at(field) = Value::Bool(value); }
    void SetInt(size_t field, int value) { values.at(field) = Value::Int(value); }
    void SetString(size_t field, std::string value) { values.at(field) = Value::Str(std::move(value)); }

    bool operator==(const Snapshot& other) const { return values == other.values; }
    bool operator!=(const Snapshot& other) const { return !(*this == other); }
};

// Reset 使用的默认值；与各配置变量的默认值一致，由字段表给出。
inline Snapshot Defaults(const FieldTable& fields) {
    Snapshot s;
    s.values.reserve(fields.size());
    for (const FieldSpec& spec : fields) s.values.push_back(spec.defaultValue);
    return s;
}

// from 到 to 之间取值不同的字段下标，按字段表顺序。两份快照长度不同时按较短者比较。
inline std::vector<size_t> Diff(const Snapshot& from, const Snapshot& to) {
    std::vector<size_t> changed;
    const size_t count = (std::min)(from.size(), to.size());
    for (size_t field = 0; field < count; ++field) {
        if (from.at(field) != to.at(field)) changed.push_back(field);
    }
    return changed;
}

inline bool IsDirty(const Snapshot& initial, const Snapshot& draft) { return initial != draft; }

// 模板名只允许字母、数字、连字符和下划线，与 CreateTemplate / SetActiveTemplateName 同一规则。
inline bool IsValidTemplateName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        // unsigned char 转型：非 ASCII 字节为负值时 isalnum 是未定义行为
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
    }
    return true;
}

struct ValidationResult {
    ValidationError error = ValidationError::None;
    size_t field = 0;

    bool ok() const { return error == ValidationError::None; }
};

// Apply 前的全量校验：按字段表顺序，第一处失败即返回，配置在此之前不会被写。
// 每个字段先查枚举范围，再跑该字段的额外校验。
inline ValidationResult Validate(const FieldTable& fields, const Snapshot& draft) {
    const size_t count = (std::min)(fields.size(), draft.size());
    for (size_t field = 0; field < count; ++field) {
        const FieldSpec& spec = fields[field];
        const Value& value = draft.at(field);
        if (spec.kind == Kind::Int && spec.enumCount > 0 && (value.i < 0 || value.i >= spec.enumCount)) {
            return {ValidationError::OutOfRange, field};
        }
        if (spec.validate) {
            const ValidationError extra = spec.validate(value);
            if (extra != ValidationError::None) return {extra, field};
        }
    }
    return {};
}

// 外部写入冲突：只看已编辑字段；current 与基线不同、且与草稿也不同，才是冲突。
// 外部恰好写成了草稿要写的值不算冲突；未编辑字段无论外部怎么改都不算。
inline std::vector<size_t> DetectConflicts(const Snapshot& initial, const Snapshot& draft, const Snapshot& current) {
    std::vector<size_t> conflicts;
    for (size_t field : Diff(initial, draft)) {
        if (field >= current.size()) continue;
        if (current.at(field) != initial.at(field) && current.at(field) != draft.at(field)) {
            conflicts.push_back(field);
        }
    }
    return conflicts;
}

// 影响重启的字段里，草稿与本进程启动值不同的任一个都要求重启。
inline bool NeedsRestart(const FieldTable& fields, const Snapshot& startup, const Snapshot& draft) {
    const size_t count = (std::min)({fields.size(), startup.size(), draft.size()});
    for (size_t field = 0; field < count; ++field) {
        if (fields[field].affectsRestart && startup.at(field) != draft.at(field)) return true;
    }
    return false;
}

// 未知存储值在 UI 中显示为安全默认值；仅 Apply 时才允许写回。
inline int SanitizeEnum(int value, int count, int fallback) {
    return (value < 0 || value >= count) ? fallback : value;
}

}  // namespace prefs_draft
