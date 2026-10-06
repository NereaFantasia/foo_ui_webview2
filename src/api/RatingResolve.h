#pragma once
// RatingResolve.h - Read rating candidates from foobar2000 and file metadata.
// Selection and clamping are defined in RatingSelect.h.
/*
 * 播放列表行、rating.get 和 metadb:changed 共用评分读取规则，避免不同出口给出
 * 不同的值。统计候选值来自 %rating%，文件标签来自传入的 file_info。
 *
 * 只能在主线程调用：缓存的 titleformat 脚本由所有调用方共用，不能并发初始化
 * 或假定脚本可跨线程求值。worker 中需要评分时，应将读取调度到主线程。
 *
 * metadb_handle_ptr 与 file_info 的完整定义由使用本头的编译单元的 pch.h 提供。
 */

#include "api/RatingSelect.h"

// 读一首曲目的评分。track 必须有效；info 为空指针表示拿不到 file_info 容器，
// 此时只读取统计候选值。
inline TrackRating ResolveTrackRating(const metadb_handle_ptr& track, const file_info* info) {
    std::optional<int> stats;
    try {
        // 通过 titleformat 的 %rating% 字段读取统计候选值。
        static titleformat_object::ptr script;
        if (!script.is_valid()) titleformat_compiler::get()->compile_safe(script, "%rating%");
        if (script.is_valid()) {
            pfc::string8 result;
            track->format_title(nullptr, result, script, nullptr);
            // 空串是没有统计值；"?" 是 titleformat 对未知字段的回答，同样不是评分。
            if (result.get_length() > 0 && result[0] != '?') stats = atoi(result.get_ptr());
        }
    } catch (...) {
        // 求值失败按统计值缺失处理；有 file_info 时仍可回退到文件标签。
    }

    std::optional<int> tag;
    if (info != nullptr) {
        // 字段名大小写不敏感，标签实际以 RATING 写入时这里同样命中。
        if (const char* value = info->meta_get("rating", 0)) tag = atoi(value);
    }

    return SelectRating(stats, tag);
}
