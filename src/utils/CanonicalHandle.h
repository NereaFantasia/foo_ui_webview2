/**
 * CanonicalHandle.h - "先规范化路径、再建 metadb handle" 的调用契约（SDK-free）
 *
 * metadb::handle_create 不做路径规范化，SDK 的约定是调用方自己做
 * （metadb.cpp 的 handle_create_replace_path 先 g_get_canonical_path 再转
 * canonical 版本）。跳过这一步的站点会与经 process_locations 解析出的同一
 * 文件产生两种 metadb 身份，去重与存在性判定随之失效。
 *
 * 本头只放不依赖 SDK 的模板，供单测用 fake 证明契约；生产代码用
 * SubsongUtils::CreateCanonicalHandle 与 SubsongUtils::CreateTrackHandle
 * （注入真实 SDK 调用）。
 */
#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include "utils/SubsongPath.h"

namespace fb2k_paths {

    // canonicalize: (const char* path) -> std::string
    // handleCreate: (const char* canonicalPath, uint32_t subsong) -> R
    // 规范化结果原样、subsong 原样交给 handleCreate；返回 handleCreate 的返回值。
    // 交给 handleCreate 的规范化结果同时写进 canonicalOut，供调用方在直读回退、
    // 错误详情里用同一个拼写，不必再规范化一次。
    template <typename Canonicalize, typename HandleCreate>
    auto CreateCanonicalHandleWith(const char* path, uint32_t subsong,
                                   Canonicalize&& canonicalize,
                                   HandleCreate&& handleCreate,
                                   std::string& canonicalOut) {
        canonicalOut = canonicalize(path);
        return handleCreate(canonicalOut.c_str(), subsong);
    }

    template <typename Canonicalize, typename HandleCreate>
    auto CreateCanonicalHandleWith(const char* path, uint32_t subsong,
                                   Canonicalize&& canonicalize,
                                   HandleCreate&& handleCreate) {
        std::string canonicalPath;
        return CreateCanonicalHandleWith(path, subsong, canonicalize, handleCreate, canonicalPath);
    }

    // 调用方给的曲目路径（可带 "|subsong:N"）建 handle：先按 SplitSubsongPath 拆掉
    // 后缀，拆出的路径经上面的模板规范化，拆出的序号随之交给 handleCreate。后缀
    // 不能留给规范化，否则会被当成文件名的一部分。parts 收到拆分结果，供调用方
    // 判断要不要为读不出的序号记日志。
    template <typename Canonicalize, typename HandleCreate>
    auto CreateTrackHandleWith(std::string_view trackPath,
                               Canonicalize&& canonicalize,
                               HandleCreate&& handleCreate,
                               SubsongPath& parts) {
        parts = SplitSubsongPath(trackPath);
        return CreateCanonicalHandleWith(parts.path.c_str(), parts.subsong, canonicalize, handleCreate);
    }

} // namespace fb2k_paths
