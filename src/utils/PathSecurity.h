#pragma once
// ============================================
// PathSecurity.h - 统一路径安全验证模块
// 安全修复: 动态信任模式
// ============================================
// 
// 策略:
//   - 非系统盘 (D:, E:, ...) 默认放行
//   - 系统盘 (C:) 仅允许白名单目录
//   - 危险目录黑名单 (Windows, System32, Program Files)
//   - 支持 UNC 网络路径 (\\Server\Share)
//   - 支持 FB2K 特殊协议 (archive://, tone://, cdda://)
//   - 上下文信任: 播放列表/媒体库中的文件自动信任
//
// ============================================

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <cwctype>
#include <ShlObj.h>
#include <foobar2000/SDK/foobar2000.h>

namespace fs = std::filesystem;

class PathSecurity {
public:
    static PathSecurity& Instance() {
        static PathSecurity instance;
        return instance;
    }

    // ========================================
    // 主要验证接口
    // ========================================
    
    // 验证路径是否允许访问 (读取)
    bool ValidatePath(const std::wstring& rawPath, std::wstring& errorMsg) {
        // 虚拟/网络协议早期放行: 不涉及本地文件系统，无需盘符/黑白名单检查
        if (IsVirtualOrNetworkProtocol(rawPath)) {
            return true;
        }
        
        std::wstring realPath;
        if (!PassBasicPathSafetyChecks(rawPath, realPath, errorMsg)) {
            return false;
        }
        
        try {
            // UNC 网络路径处理
            if (IsUNCPath(realPath)) {
                return ValidateUNCPath(realPath, errorMsg);
            }
            
            // 获取盘符
            if (realPath.length() < 2 || realPath[1] != L':') {
                errorMsg = L"Invalid path format";
                return false;
            }
            
            wchar_t drive = ::towupper(realPath[0]);
            
            // 非系统盘: 默认放行
            if (drive != systemDrive_) {
                return true;
            }
            
            // 系统盘: 危险目录黑名单检查
            if (IsInBlacklist(realPath)) {
                errorMsg = L"Access denied: protected system path";
                return false;
            }
            
            // 系统盘: 白名单检查
            if (IsInWhitelist(realPath)) {
                return true;
            }
            
            // 系统盘其他路径: 默认拒绝
            errorMsg = L"Access denied: system drive path not in whitelist";
            return false;
            
        } catch (const std::exception&) {
            errorMsg = L"Path validation error";
            return false;
        }
    }
    
    // 验证写入路径 (比读取更严格)
    bool ValidateWritePath(const std::wstring& rawPath, std::wstring& errorMsg) {
        // 基础验证
        if (!ValidatePath(rawPath, errorMsg)) {
            return false;
        }
        
        std::wstring path = PreprocessProtocolPath(rawPath);
        
        // 解析真实路径
        std::wstring realPath;
        try {
            if (fs::exists(path)) {
                realPath = fs::canonical(path).wstring();
            } else {
                realPath = fs::weakly_canonical(path).wstring();
            }
        } catch (...) {
            realPath = path;
        }
        
        // 写入仅允许: profile 目录和 temp 目录
        std::wstring lowerPath = realPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
        
        for (const auto& allowed : writeAllowedDirs_) {
            std::wstring lowerAllowed = allowed;
            std::transform(lowerAllowed.begin(), lowerAllowed.end(), lowerAllowed.begin(), ::towlower);
            if (lowerPath.find(lowerAllowed) == 0) {
                return true;
            }
        }
        
        errorMsg = L"Write access denied: only profile and temp directories allowed";
        return false;
    }
    
    // 验证媒体访问 (上下文信任)
    bool ValidateMediaAccess(const std::wstring& path, std::wstring& errorMsg) {
        // 首先尝试基础路径验证
        if (ValidatePath(path, errorMsg)) {
            return true;
        }
        
        // 快速检查: 路径是否在媒体库监视目录覆盖范围内
        try {
            std::string utf8 = pfc::stringcvt::string_utf8_from_wide(path.c_str()).get_ptr();
            if (library_manager::get()->is_path_addable(utf8.c_str())) {
                errorMsg.clear();
                return true;
            }
        } catch (...) {}
        
        // 回退: 检查是否在播放列表或媒体库中
        if (IsItemInLibraryOrPlaylist(path)) {
            errorMsg.clear();
            return true;  // 上下文信任
        }
        
        return false;
    }

    // 验证媒体写入 (比 MediaRead 更严格: 不因"非系统盘"放行写入)
    bool ValidateMediaWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg,
                                  const std::wstring& contextMediaPath = L"") {
        std::wstring realPath;
        if (!PassBasicPathSafetyChecks(rawPath, realPath, errorMsg)) {
            return false;
        }
        
        // 0. 黑名单检查 (防止库中被注入的系统路径绕过)
        if (IsInBlacklist(realPath)) {
            errorMsg = L"Write access denied: protected system path";
            return false;
        }
        
        // 1. 现有严格写白名单 (profile/temp)
        std::wstring lowerPath = realPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
        
        for (const auto& allowed : writeAllowedDirs_) {
            std::wstring lowerAllowed = allowed;
            std::transform(lowerAllowed.begin(), lowerAllowed.end(), lowerAllowed.begin(), ::towlower);
            if (lowerPath.find(lowerAllowed) == 0) {
                return true;
            }
        }
        
        // 2. 非系统盘直通 (与 ValidatePath 读取策略对称)
        // 系统盘 (通常 C:) 仍走媒体库上下文信任, 防止写入系统盘任意路径
        if (!IsUNCPath(realPath) && realPath.length() >= 2 && realPath[1] == L':') {
            wchar_t drive = ::towupper(realPath[0]);
            if (drive != systemDrive_) {
                return true;
            }
        }
        
        // 3. 受信任媒体上下文 (媒体库/播放列表) — 覆盖系统盘的媒体文件
        if (IsItemInLibraryOrPlaylist(rawPath)) {
            return true;
        }
        
        // 3b. 同目录派生文件信任: 写入路径与受信上下文音频文件在同一父目录
        if (!contextMediaPath.empty()) {
            try {
                auto writtenDir = std::filesystem::path(realPath).parent_path();
                auto mediaDir = std::filesystem::path(contextMediaPath).parent_path();
                if (std::filesystem::equivalent(writtenDir, mediaDir) &&
                    IsItemInLibraryOrPlaylist(contextMediaPath)) {
                    return true;
                }
            } catch (...) {}
        }
        
        // 4. 后续扩展点: trusted media roots (暂不启用)
        // if (IsInTrustedMediaRoots(realPath)) { return true; }
        
        errorMsg = L"Write access denied: system drive path is not in trusted media context";
        return false;
    }
    
    // 简化接口
    bool IsPathSafe(const std::wstring& path) {
        std::wstring errorMsg;
        return ValidatePath(path, errorMsg);
    }

private:
    PathSecurity() {
        InitializeSystemDrive();
        InitializeWhitelist();
        InitializeBlacklist();
        InitializeWriteAllowedDirs();
    }
    
    wchar_t systemDrive_ = L'C';
    std::vector<std::wstring> whitelist_;
    std::vector<std::wstring> blacklist_;
    std::vector<std::wstring> writeAllowedDirs_;
    
    // ========================================
    // 初始化
    // ========================================
    
    void InitializeSystemDrive() {
        wchar_t winDir[MAX_PATH];
        if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
            systemDrive_ = ::towupper(winDir[0]);
        }
    }
    
    void InitializeWhitelist() {
        whitelist_.clear();
        
        // FB2K profile 目录
        whitelist_.push_back(GetFoobarProfilePath());
        
        // 用户音乐目录
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYMUSIC, nullptr, 0, path))) {
            whitelist_.push_back(path);
        }
        
        // 用户桌面 (用户常放歌的位置)
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path))) {
            whitelist_.push_back(path);
        }
        
        // 用户文档
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, path))) {
            whitelist_.push_back(path);
        }
        
        // 用户下载目录
        PWSTR downloadPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadPath))) {
            whitelist_.push_back(downloadPath);
            CoTaskMemFree(downloadPath);
        }
        
        // Temp 目录
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) > 0) {
            whitelist_.push_back(tempPath);
        }
        
        // 便携版: 添加 FB2K 安装目录
        std::wstring installDir = GetFoobarInstallPath();
        if (!installDir.empty()) {
            whitelist_.push_back(installDir);
        }
        
        // 用户视频目录
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYVIDEO, nullptr, 0, path))) {
            whitelist_.push_back(path);
        }
        
        // OneDrive 目录 (很多用户在 OneDrive 同步音乐库)
        PWSTR oneDrivePath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SkyDrive, 0, nullptr, &oneDrivePath))) {
            whitelist_.push_back(oneDrivePath);
            CoTaskMemFree(oneDrivePath);
        }
    }
    
    void InitializeBlacklist() {
        blacklist_.clear();
        
        wchar_t path[MAX_PATH];
        
        // Windows 目录
        if (GetWindowsDirectoryW(path, MAX_PATH) > 0) {
            blacklist_.push_back(path);
        }
        
        // System32
        if (GetSystemDirectoryW(path, MAX_PATH) > 0) {
            blacklist_.push_back(path);
        }
        
        // Program Files
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, path))) {
            blacklist_.push_back(path);
        }
        
        // Program Files (x86)
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, path))) {
            blacklist_.push_back(path);
        }
        
        // ProgramData
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, path))) {
            blacklist_.push_back(path);
        }
    }
    
    void InitializeWriteAllowedDirs() {
        writeAllowedDirs_.clear();
        
        // Profile 目录
        writeAllowedDirs_.push_back(GetFoobarProfilePath());
        
        // Temp 目录
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) > 0) {
            writeAllowedDirs_.push_back(tempPath);
        }
    }
    
    // ========================================
    // 辅助函数
    // ========================================
    
    // 基础路径安全检查 (从 ValidatePath 提取的公共逻辑)
    // 负责: 空路径检查、虚拟协议放行、协议预处理、遍历攻击检测、符号链接解析、UNC/盘符格式验证
    // 不负责: 黑白名单、系统盘策略、上下文信任 — 这些由调用者自行决定
    bool PassBasicPathSafetyChecks(const std::wstring& rawPath, std::wstring& resolvedPath, std::wstring& errorMsg) {
        try {
            if (rawPath.empty()) {
                errorMsg = L"Empty path";
                return false;
            }
            
            // 虚拟/网络协议早期放行: 不涉及本地文件系统，无需路径安全检查
            if (IsVirtualOrNetworkProtocol(rawPath)) {
                resolvedPath = rawPath;
                return true;
            }
            
            std::wstring path = PreprocessProtocolPath(rawPath);
            if (path.empty()) {
                errorMsg = L"Invalid protocol path";
                return false;
            }
            
            // 设备路径拦截: \\.\ 和 \\?\ 前缀可绕过盘符/黑名单检查
            if (path.starts_with(L"\\\\.\\") || path.starts_with(L"\\\\?\\") ||
                path.starts_with(L"\\\\.\\.") || path.starts_with(L"\\\\?\\.")) {
                errorMsg = L"Device paths are not allowed";
                return false;
            }
            
            if (ContainsTraversal(path)) {
                errorMsg = L"Path traversal detected";
                return false;
            }
            
            // 解析符号链接到真实路径 (防止符号链接绕过)
            try {
                if (fs::exists(path)) {
                    resolvedPath = fs::canonical(path).wstring();
                } else {
                    resolvedPath = fs::weakly_canonical(path).wstring();
                }
            } catch (...) {
                resolvedPath = path;
            }
            
            // 展开 8.3 短文件名 (防止 PROGRA~1 等绕过黑名单)
            wchar_t longPath[MAX_PATH];
            DWORD len = GetLongPathNameW(resolvedPath.c_str(), longPath, MAX_PATH);
            if (len > 0 && len < MAX_PATH) {
                resolvedPath = longPath;
            }
            
            return true;
        } catch (const std::exception&) {
            errorMsg = L"Path validation error";
            return false;
        }
    }
    
    std::wstring GetFoobarProfilePath() {
        try {
            pfc::string8 profilePath = core_api::get_profile_path();
            // 移除 file:// 前缀
            if (profilePath.startsWith("file://")) {
                profilePath = profilePath.subString(7);
            }
            return pfc::stringcvt::string_wide_from_utf8(profilePath.c_str()).get_ptr();
        } catch (...) {
            return L"";
        }
    }
    
    std::wstring GetFoobarInstallPath() {
        try {
            pfc::string8 myPath = core_api::get_my_full_path();
            // 移除 file:// 前缀
            if (myPath.startsWith("file://")) {
                myPath = myPath.subString(7);
            }
            std::wstring wpath = pfc::stringcvt::string_wide_from_utf8(myPath.c_str()).get_ptr();
            
            // 获取父目录 (components 目录的父目录)
            fs::path p(wpath);
            if (p.has_parent_path()) {
                p = p.parent_path();  // components
                if (p.has_parent_path()) {
                    return p.parent_path().wstring();  // foobar2000 目录
                }
            }
            return L"";
        } catch (...) {
            return L"";
        }
    }
    
    // 检测不涉及本地文件系统的虚拟/网络协议
    // 这些协议的路径不需要文件系统安全检查（黑白名单、符号链接解析等）
    // 通用规则: 任何含 :// 且非 file:// / archive:// / unpack:// 的路径
    // 视为虚拟/网络协议放行 (foobar2000 第三方输入组件使用自定义协议)
    bool IsVirtualOrNetworkProtocol(const std::wstring& path) {
        // 只检查前 30 个字符以提高性能
        size_t checkLen = (std::min)(path.length(), static_cast<size_t>(30));
        std::wstring prefix(path.begin(), path.begin() + checkLen);
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::towlower);
        
        // 这些协议内嵌本地文件路径，需要走完整路径安全检查
        if (prefix.starts_with(L"file://") ||
            prefix.starts_with(L"archive://") ||
            prefix.starts_with(L"unpack://")) {
            return false;
        }
        
        // 通用 :// 协议检测 — 覆盖所有第三方输入组件自定义协议
        size_t schemeEnd = prefix.find(L"://");
        if (schemeEnd != std::wstring::npos && schemeEnd > 0 && schemeEnd < 20) {
            return true;
        }
        
        return false;
    }
    
    // 预处理 FB2K 特殊协议路径
    std::wstring PreprocessProtocolPath(const std::wstring& path) {
        // 检测特殊协议前缀
        static const std::vector<std::wstring> protocols = {
            L"archive://",   // 压缩包内文件
            L"unpack://",    // 解压文件
            L"tone://",      // 音轨
            L"cdda://",      // CD 音轨
            L"file://",      // 本地文件
        };
        
        for (const auto& proto : protocols) {
            if (path.find(proto) == 0) {
                std::wstring extracted = path.substr(proto.length());
                
                // archive://D:\Music\Album.zip|/Track01.flac
                // 提取 | 之前的部分
                size_t pipePos = extracted.find(L'|');
                if (pipePos != std::wstring::npos) {
                    extracted = extracted.substr(0, pipePos);
                }
                
                // 处理 URL 编码
                // 简单处理常见编码 (完整实现需要 URL decode)
                // 完整 URL 解码暂未实现。
                
                return extracted;
            }
        }
        
        return path;
    }
    
    bool ContainsTraversal(const std::wstring& path) {
        return path.find(L"..") != std::wstring::npos ||
               path.find(L"./") != std::wstring::npos ||
               path.find(L".\\") != std::wstring::npos;
    }
    
    bool IsUNCPath(const std::wstring& path) {
        return path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\';
    }
    
    bool ValidateUNCPath(const std::wstring& path, std::wstring& errorMsg) {
        // 禁止设备路径
        if (path.find(L"\\\\.\\") == 0 || path.find(L"\\\\?\\") == 0) {
            errorMsg = L"Device paths not allowed";
            return false;
        }
        
        // UNC 网络路径允许 (NAS 支持)
        return true;
    }
    
    // 安全的目录前缀比较: 要求 path 与 prefix 精确匹配或 path 在 prefix 目录内部
    static bool IsPathPrefixOf(const std::wstring& prefix, const std::wstring& path) {
        if (path.size() < prefix.size()) return false;
        if (_wcsnicmp(path.c_str(), prefix.c_str(), prefix.size()) != 0) return false;
        if (path.size() == prefix.size()) return true;
        // 前缀本身以分隔符结尾（如 "C:\Windows\"）时直接视为匹配
        wchar_t lastPrefixChar = prefix.back();
        if (lastPrefixChar == L'\\' || lastPrefixChar == L'/') return true;
        // 否则下一个字符必须是路径分隔符
        wchar_t next = path[prefix.size()];
        return next == L'\\' || next == L'/';
    }

    bool IsInBlacklist(const std::wstring& path) {
        std::wstring lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
        
        for (const auto& blocked : blacklist_) {
            std::wstring lowerBlocked = blocked;
            std::transform(lowerBlocked.begin(), lowerBlocked.end(), lowerBlocked.begin(), ::towlower);
            
            if (IsPathPrefixOf(lowerBlocked, lowerPath)) {
                return true;
            }
        }
        return false;
    }
    
    bool IsInWhitelist(const std::wstring& path) {
        std::wstring lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
        
        for (const auto& allowed : whitelist_) {
            std::wstring lowerAllowed = allowed;
            std::transform(lowerAllowed.begin(), lowerAllowed.end(), lowerAllowed.begin(), ::towlower);
            
            if (IsPathPrefixOf(lowerAllowed, lowerPath)) {
                return true;
            }
        }
        return false;
    }
    
    // 检查文件是否在媒体库或播放列表中 (上下文信任)
    // 修复: 遍历全部播放列表 / 规范化路径比较 / 忽略 subsong index
    bool IsItemInLibraryOrPlaylist(const std::wstring& path) {
        try {
            // 转换为 UTF8 并规范化路径 (file://... 格式)
            std::string utf8Path = pfc::stringcvt::string_utf8_from_wide(path.c_str()).get_ptr();
            pfc::string8 canonicalPath;
            filesystem::g_get_canonical_path(utf8Path.c_str(), canonicalPath);
            
            // 1. 检查媒体库 (subsong 0 覆盖绝大多数非 CUE 文件)
            auto library = library_manager::get();
            metadb_handle_ptr handle;
            metadb::get()->handle_create(handle, make_playable_location(canonicalPath.c_str(), 0));
            if (handle.is_valid() && library->is_item_in_library(handle)) {
                return true;
            }
            
            // 2. 遍历所有播放列表, 规范化路径比较 (忽略 subsong index)
            auto pm = playlist_manager::get();
            t_size plCount = pm->get_playlist_count();
            t_size totalChecked = 0;
            static constexpr t_size kMaxTotalItems = 50000;
            
            for (t_size pl = 0; pl < plCount && totalChecked < kMaxTotalItems; ++pl) {
                t_size itemCount = pm->playlist_get_item_count(pl);
                for (t_size i = 0; i < itemCount && totalChecked < kMaxTotalItems; ++i, ++totalChecked) {
                    metadb_handle_ptr item;
                    if (pm->playlist_get_item_handle(item, pl, i)) {
                        if (metadb::path_compare(canonicalPath.c_str(), item->get_path()) == 0) {
                            return true;
                        }
                    }
                }
            }
            
            return false;
        } catch (...) {
            return false;
        }
    }
};

// 便捷函数
inline bool IsPathSafe(const std::wstring& path) {
    return PathSecurity::Instance().IsPathSafe(path);
}

inline bool ValidatePath(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidatePath(path, errorMsg);
}

inline bool ValidateWritePath(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateWritePath(path, errorMsg);
}

inline bool ValidateMediaAccess(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateMediaAccess(path, errorMsg);
}

inline bool ValidateMediaWriteAccess(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateMediaWriteAccess(path, errorMsg);
}
