/**
 * CanonicalHandle.h - "先规范化路径、再建 metadb handle" 的调用契约（SDK-free）
 *
 * metadb::handle_create 不做路径规范化，SDK 的约定是调用方自己做
 * （metadb.cpp 的 handle_create_replace_path 先 g_get_canonical_path 再转
 * canonical 版本）。跳过这一步的站点会与经 process_locations 解析出的同一
 * 文件产生两种 metadb 身份，去重与存在性判定随之失效。
 *
 * 本头只放不依赖 SDK 的模板，供单测用 fake 证明契约；生产代码用
 * SubsongUtils::CreateCanonicalHandle（注入真实 SDK 调用）。
 */
#pragma once
#include <cstdint>
#include <string>

namespace fb2k_paths {

    // canonicalize: (const char* path) -> std::string
    // handleCreate: (const char* canonicalPath, uint32_t subsong) -> R
    // 规范化结果原样、subsong 原样交给 handleCreate；返回 handleCreate 的返回值。
    template <typename Canonicalize, typename HandleCreate>
    auto CreateCanonicalHandleWith(const char* path, uint32_t subsong,
                                   Canonicalize&& canonicalize,
                                   HandleCreate&& handleCreate) {
        const std::string canonicalPath = canonicalize(path);
        return handleCreate(canonicalPath.c_str(), subsong);
    }

} // namespace fb2k_paths
