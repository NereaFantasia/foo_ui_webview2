#pragma once
// ============================================================
// CloudState — 云占位符判定与批次输入规则（纯逻辑，无 SDK / 无文件 IO）
// ============================================================
//
// 为什么单独成头：`cloud.getStates` 的判据（属性位 → local/placeholder）与
// 参数选择规则（paths / directory 恰好一个、批量上限）应当能在 gtest 里
// 直接测，而不必起 foobar2000。handler 只负责拿 Win32 属性并填结构体。
//
// 红线口径：这里只做**元数据**判定。文件属性读取（GetFileAttributesW /
// FindFirstFileW 的 WIN32_FIND_DATA）不打开文件内容、不触发 OneDrive 水合；
// 判出 placeholder 的目的是让调用方**避开**读内容，而不是去读它。
// ============================================================

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cloud_state {

// Win32 FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS：OneDrive 云占位符（在线可用）标志。
// 与 winnt.h 的同名宏同值；此头不引 Windows.h，故按字面量定义并在可用时静态核对。
inline constexpr std::uint32_t kFileAttributeRecallOnDataAccess = 0x00400000u;

#ifdef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
static_assert(kFileAttributeRecallOnDataAccess == FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS,
              "FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS changed; update cloud_state");
#endif

// 单批路径上限（与 schema 文档一致）。超过时整次调用以 INVALID_PARAMS 失败，
// 而不是静默截断：静默截断会让调用方以为剩余路径已检查。
inline constexpr std::size_t kMaxBatchPaths = 256;

struct PathState {
    bool exists = false;
    bool local = false;
    bool placeholder = false;
    std::uint32_t attrs = 0;
};

// 由 GetFileAttributesW 的结果判定。`exists` 为 false 时其余字段保持 false / 0。
// `local` 的精确定义：exists && !(attrs & RECALL_ON_DATA_ACCESS)。
inline PathState Classify(bool exists, std::uint32_t attrs) {
    PathState state;
    state.exists = exists;
    state.attrs = exists ? attrs : 0;
    if (!exists) return state;
    state.placeholder = (attrs & kFileAttributeRecallOnDataAccess) != 0;
    state.local = !state.placeholder;
    return state;
}

// 参数选择规则：`paths` 与 `directory` 恰好给一个；paths 模式必须非空且不超上限。
// （`directory` 模式的路径本身由 schema 的 @minLength 1 保证非空。）
enum class SelectionError {
    None = 0,
    BothGiven,     // paths 与 directory 同时给
    NeitherGiven,  // 两个都没给
    EmptyPaths,    // paths 给了空数组
    TooManyPaths,  // paths 超过 kMaxBatchPaths
};

inline SelectionError ValidateSelection(bool hasPaths, bool hasDirectory, std::size_t pathCount) {
    if (hasPaths && hasDirectory) return SelectionError::BothGiven;
    if (!hasPaths && !hasDirectory) return SelectionError::NeitherGiven;
    if (hasPaths) {
        if (pathCount == 0) return SelectionError::EmptyPaths;
        if (pathCount > kMaxBatchPaths) return SelectionError::TooManyPaths;
    }
    return SelectionError::None;
}

inline std::string_view SelectionErrorName(SelectionError error) {
    switch (error) {
        case SelectionError::None:         return "none";
        case SelectionError::BothGiven:    return "both-given";
        case SelectionError::NeitherGiven: return "neither-given";
        case SelectionError::EmptyPaths:   return "empty-paths";
        case SelectionError::TooManyPaths: return "too-many-paths";
    }
    return "unknown";
}

}  // namespace cloud_state
