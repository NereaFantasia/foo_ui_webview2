/**
 * Cloud API — 云端状态只读查询
 *
 * 背景：宿主内部早已用 GetFileAttributesW + FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
 * 判 OneDrive 云占位符（artwork 的占位符防御），但页面拿不到这个判断，主题只能
 * 用 shell.exec 跑 PowerShell（cloudProbe / scan-cloud-state.ps1）逐文件探测。
 * cloud.getStates 把这个**只读元数据**判断直接暴露成同步批接口。
 *
 * 红线：本文件只调用 GetFileAttributesW（目录模式另用 FindFirstFileW 取目录项属性），
 * 不打开、不读取任何文件内容 ⇒ 不触发水合；属性读取本身只走本地元数据。
 */

#include "pch.h"
#include "api/CloudApi.h"
#include "api/CloudState.h"
#include "api/TypedApi.h"
#include "api/generated/CloudSchema.h"
#include "utils/SubsongUtils.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace cloud = api::cloud;

// 把页面给的路径（native / file:// / file-relative://，可带 |subsong:N）解析成
// Win32 文件 API 能接受的本地原生路径。非本地路径（http://、archive:// 等）返回
// false —— 它们没有可查的本地属性，按 exists:false 报告。
bool ResolveNativePathForAttributes(const std::string& path, std::string& nativePath) {
    const auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(path);
    (void)subsong;  // 属性属于整个文件，子曲目后缀对属性读取没有意义
    nativePath.clear();
    if (filePath.empty()) return false;
    return SubsongUtils::TryResolveNativeMediaPath(filePath, nativePath);
}

// 单个路径的元数据判定：不存在 / 非本地路径都是 exists:false，不是错误。
cloud::CloudPathState InspectPath(const std::string& givenPath) {
    cloud::CloudPathState state;
    state.path = givenPath;

    std::string nativePath;
    if (!ResolveNativePathForAttributes(givenPath, nativePath)) {
        return state;
    }

    const DWORD attrs = GetFileAttributesW(Utf8ToWide(nativePath).c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return state;
    }

    const cloud_state::PathState classified =
        cloud_state::Classify(true, static_cast<std::uint32_t>(attrs));
    state.exists = classified.exists;
    state.local = classified.local;
    state.placeholder = classified.placeholder;
    state.attrs = static_cast<std::int64_t>(classified.attrs);
    return state;
}

std::wstring JoinChildPath(const std::wstring& directory, const std::wstring& name) {
    std::wstring child = directory;
    if (!child.empty() && child.back() != L'\\' && child.back() != L'/') {
        child.push_back(L'\\');
    }
    child += name;
    return child;
}

// 目录模式：只列直接子项（不递归），每项的 local/placeholder 由 WIN32_FIND_DATA 的
// 属性位判定。目录不存在 → NOT_FOUND；不是目录 → INVALID_PATH（与 file.list 一致）。
api::Result<cloud::GetStatesResult> EnumerateDirectory(const std::string& directory) {
    cloud::GetStatesResult result;

    std::string nativeDirectory;
    if (!ResolveNativePathForAttributes(directory, nativeDirectory)) {
        return api::Fail("directory is not a local filesystem path", ApiErrorCode::INVALID_PATH);
    }

    const std::wstring wideDirectory = Utf8ToWide(nativeDirectory);
    const DWORD directoryAttrs = GetFileAttributesW(wideDirectory.c_str());
    if (directoryAttrs == INVALID_FILE_ATTRIBUTES) {
        return api::Fail("directory not found", ApiErrorCode::NOT_FOUND);
    }
    if ((directoryAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return api::Fail("path is not a directory", ApiErrorCode::INVALID_PATH);
    }

    const std::wstring searchPattern = JoinChildPath(wideDirectory, L"*");
    WIN32_FIND_DATAW findData{};
    HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        // 目录可读但列不出来（权限/竞态）：与"空目录"同形返回，不升级成错误。
        LOG("cloud.getStates: directory listing failed for %s", directory.c_str());
        return result;
    }

    do {
        const std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        cloud::CloudPathState state;
        state.path = WideToUtf8(JoinChildPath(wideDirectory, name));

        const cloud_state::PathState classified =
            cloud_state::Classify(true, static_cast<std::uint32_t>(findData.dwFileAttributes));
        state.exists = classified.exists;
        state.local = classified.local;
        state.placeholder = classified.placeholder;
        state.attrs = static_cast<std::int64_t>(classified.attrs);
        result.states.push_back(std::move(state));
    } while (FindNextFileW(findHandle, &findData));

    FindClose(findHandle);

    // 目录枚举顺序由文件系统决定；排序让调用方与探针拿到稳定顺序。
    std::sort(result.states.begin(), result.states.end(),
              [](const cloud::CloudPathState& left, const cloud::CloudPathState& right) {
                  return left.path < right.path;
              });
    return result;
}

api::Result<cloud::GetStatesResult> CloudGetStates(const cloud::GetStatesParams& p) {
    const bool hasPaths = p.paths.has_value();
    const bool hasDirectory = p.directory.has_value();
    const std::size_t pathCount = hasPaths ? p.paths->size() : 0;

    switch (cloud_state::ValidateSelection(hasPaths, hasDirectory, pathCount)) {
        case cloud_state::SelectionError::None:
            break;
        case cloud_state::SelectionError::BothGiven:
            return api::Fail("exactly one of paths or directory is required", ApiErrorCode::INVALID_PARAMS);
        case cloud_state::SelectionError::NeitherGiven:
            return api::Fail("one of paths or directory is required", ApiErrorCode::INVALID_PARAMS);
        case cloud_state::SelectionError::EmptyPaths:
            return api::Fail("paths must not be empty", ApiErrorCode::INVALID_PARAMS);
        case cloud_state::SelectionError::TooManyPaths:
            return api::Fail("paths exceeds the 256-entry limit", ApiErrorCode::INVALID_PARAMS);
    }

    if (hasPaths) {
        cloud::GetStatesResult result;
        result.states.reserve(pathCount);
        for (const std::string& path : *p.paths) {
            result.states.push_back(InspectPath(path));
        }
        return result;
    }

    return EnumerateDirectory(*p.directory);
}

}  // namespace

void RegisterCloudApi() {
    // 路径参数的安全级别来自声明（生成头里的 kPathParams：paths[]/directory 均 MediaRead）。
    api::RegisterApi("cloud.getStates", CloudGetStates);
    LOG("Cloud API registered (1 API)");
}
