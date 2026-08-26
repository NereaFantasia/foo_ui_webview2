#pragma once
// ============================================
// PathSecurity.h - 统一路径安全验证模块
// 动态信任模式
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
#include <cstdint>
#include <optional>
#include <ShlObj.h>
#include <foobar2000/SDK/foobar2000.h>
#include "utils/MediaMembershipIndex.h"
#include "utils/PathCanonicalCache.h"
#include "utils/PathCanonicalForm.h"

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
        if (IsInWriteWhitelist(realPath)) {
            return true;
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
        if (IsInTrustedMediaRoots(path)) {
            errorMsg.clear();
            return true;
        }
        
        // 回退: 检查是否在播放列表或媒体库中
        if (IsItemInLibraryOrPlaylist(path)) {
            errorMsg.clear();
            return true;  // 上下文信任
        }
        
        return false;
    }

    // 验证媒体写入: 只允许写进用户已显式纳入媒体上下文的位置。
    //
    // 比 MediaRead 更严格 —— 读侧的"非系统盘默认放行"不得继承到写侧,
    // 否则等于"只要在 D:/E: 上就能改任意音频文件"。
    //
    // 允许集合恰为四类: 严格写白名单 (profile/temp)、媒体库或播放列表中的
    // 文件、媒体库监视目录覆盖范围内的文件、以及与受信上下文音频同目录的
    // 派生伴生文件。其余一律拒绝。
    bool ValidateMediaWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg,
                                  const std::wstring& contextMediaPath = L"") {
        std::wstring realPath;
        if (!PassBasicPathSafetyChecks(rawPath, realPath, errorMsg)) {
            return false;
        }
        
        // 黑名单必须最先判, 且判的是 realPath: 经 junction 指向系统目录的
        // 路径在此被拦下, 后续各步便无需各自防御该绕过通道。
        if (IsInBlacklist(realPath)) {
            errorMsg = L"Write access denied: protected system path";
            return false;
        }
        
        // 1. 严格写白名单 (profile/temp)
        if (IsInWriteWhitelist(realPath)) {
            return true;
        }
        
        // 2. 受信任媒体上下文 (媒体库/播放列表)
        if (IsItemInLibraryOrPlaylist(rawPath)) {
            return true;
        }
        
        // 3. 媒体库监视目录 — 覆盖已落盘但尚未扫描入库的文件
        if (IsInTrustedMediaRoots(rawPath)) {
            return true;
        }
        
        // 4. 同目录派生文件信任: 写入路径与受信上下文音频文件在同一父目录。
        // 伴生文件 (.lrc 等) 本身不在库中, 故须以源音频的上下文判定。
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
        
        errorMsg = L"Write access denied: path is not in trusted media context";
        return false;
    }
    
    // 验证通用文件写入 (file.* 端点)。
    //
    // 与 ValidateMediaWriteAccess 的差别: 本函数多"非系统盘直通"、少同目录
    // 伴生文件信任, 且监视目录步排在直通之前。file.mkdir 建的新目录、
    // file.write 建的新文件都不可能预先存在于媒体库或播放列表中, 只靠这两个
    // 信任源会使整族端点失效。
    //
    // 该直通是已登记的待决项, 不是本函数的目标态: 统一权限架构 §7.2 要求
    // file.write / delete / mkdir / copy.destination / move / rename 归入
    // 严格写白名单 (profile/temp)。改归之后, 非系统盘任意路径的写入与删除
    // 将被拒绝 —— 那是公开 SDK 文档当前明确承诺的行为, 属破坏性变更, 须随
    // 版本策略单独决策。此处先把它从媒体写入语义中摘出, 使其成为显式登记的
    // 独立策略, 而非搭媒体写入的便车。
    //
    // 监视目录信任源排在该直通**之前**是为了决定步稳定, 不是为了功能存续:
    // 本链除黑名单外每步都只在命中时 return true, 没有一步在未命中时中断,
    // 故位置不改变放行集合 —— 系统盘监视目录内的路径排在直通之后同样会命中
    // (IsOnNonSystemDrive 对系统盘为 false, 遮不住它)。排在之前的收益是删
    // 直通后由监视目录接管决定步, 判定归因与顺序用例断言都不变。
    bool ValidateFileWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg) {
        std::wstring realPath;
        if (!PassBasicPathSafetyChecks(rawPath, realPath, errorMsg)) {
            return false;
        }
        
        if (IsInBlacklist(realPath)) {
            errorMsg = L"Write access denied: protected system path";
            return false;
        }
        
        if (IsInWriteWhitelist(realPath)) {
            return true;
        }
        
        // 接 canonical 之后的 realPath: 判定落点必须等于实际写入落点。用
        // rawPath 判定时, 监视目录内的重解析点 (junction/symlink) 能把写入
        // 重定向到监视目录之外、又不在黑名单内的位置 (如启动项目录), 而该位
        // 置直接传入时是被拒绝的。与下方 8.3 展开处"判据必须取 canonical 之
        // 后"是同一道理。realPath 对本步的两类正当输入均可用: 新建路径走
        // weakly_canonical 逐级解析、不存在的尾段原样保留; UNC 跳过解析、
        // 原样返回。
        if (IsInTrustedMediaRoots(realPath)) {
            return true;
        }
        
        if (IsOnNonSystemDrive(realPath)) {
            return true;
        }
        
        if (IsItemInLibraryOrPlaylist(rawPath)) {
            return true;
        }
        
        errorMsg = L"Write access denied: system drive path is not in trusted media context";
        return false;
    }
    
    // 简化接口
    bool IsPathSafe(const std::wstring& path) {
        std::wstring errorMsg;
        return ValidatePath(path, errorMsg);
    }
    
    // 作废播放列表成员索引。必须由改变播放列表成员集合的每个 SDK 回调调用,
    // 否则索引会持续返回陈旧的信任判定。下次查询时懒重建。
    void InvalidatePlaylistIndex() {
        playlistIndex_.Invalidate();
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

    // 名单条目入表口: 统一规范化并丢弃空条目。
    //
    // 规范化依赖 systemDrive_, 故构造函数必须先跑 InitializeSystemDrive
    // 再跑三个建表函数, 该顺序不可调换。
    //
    // 空条目必须丢弃: GetFoobarProfilePath / GetFoobarInstallPath 失败时返回空串,
    // 而 IsPathPrefixOf 对空 prefix 会读 prefix.back() —— 空字符串上是未定义行为,
    // 且长度 0 的前缀在逻辑上匹配一切路径, 是 fail-open 方向。
    void PushNormalized(std::vector<std::wstring>& list, const std::wstring& raw) {
        if (raw.empty()) return;
        std::wstring normalized = path_canonical::ResolveToCanonicalForm(raw, systemDrive_);
        if (normalized.empty()) return;
        list.push_back(std::move(normalized));
    }

    // 所有条目经 PushNormalized 入表: 名单必须存 canonical + 8.3 展开后的形态,
    // 才能与判定侧的 realPath 前缀匹配 (见 ResolveToCanonicalForm 注释)。
    void InitializeWhitelist() {
        whitelist_.clear();

        // FB2K profile 目录
        PushNormalized(whitelist_, GetFoobarProfilePath());

        // 用户音乐目录
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYMUSIC, nullptr, 0, path))) {
            PushNormalized(whitelist_, path);
        }

        // 用户桌面 (用户常放歌的位置)
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path))) {
            PushNormalized(whitelist_, path);
        }

        // 用户文档
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, path))) {
            PushNormalized(whitelist_, path);
        }

        // 用户下载目录
        PWSTR downloadPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &downloadPath))) {
            PushNormalized(whitelist_, downloadPath);
            CoTaskMemFree(downloadPath);
        }

        // Temp 目录
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) > 0) {
            PushNormalized(whitelist_, tempPath);
        }

        // 便携版: 添加 FB2K 安装目录
        std::wstring installDir = GetFoobarInstallPath();
        if (!installDir.empty()) {
            PushNormalized(whitelist_, installDir);
        }

        // 用户视频目录
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYVIDEO, nullptr, 0, path))) {
            PushNormalized(whitelist_, path);
        }

        // OneDrive 目录 (很多用户在 OneDrive 同步音乐库)
        PWSTR oneDrivePath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SkyDrive, 0, nullptr, &oneDrivePath))) {
            PushNormalized(whitelist_, oneDrivePath);
            CoTaskMemFree(oneDrivePath);
        }
    }
    
    void InitializeBlacklist() {
        blacklist_.clear();

        wchar_t path[MAX_PATH];

        // Windows 目录
        if (GetWindowsDirectoryW(path, MAX_PATH) > 0) {
            PushNormalized(blacklist_, path);
        }

        // System32
        if (GetSystemDirectoryW(path, MAX_PATH) > 0) {
            PushNormalized(blacklist_, path);
        }

        // Program Files
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, path))) {
            PushNormalized(blacklist_, path);
        }

        // Program Files (x86)
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, path))) {
            PushNormalized(blacklist_, path);
        }

        // ProgramData
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, path))) {
            PushNormalized(blacklist_, path);
        }
    }

    void InitializeWriteAllowedDirs() {
        writeAllowedDirs_.clear();

        // Profile 目录
        PushNormalized(writeAllowedDirs_, GetFoobarProfilePath());

        // Temp 目录
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) > 0) {
            PushNormalized(writeAllowedDirs_, tempPath);
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
            
            // UNC 网络路径: 跳过文件系统解析 (性能关键路径)
            //
            // 下方 fs::exists / fs::canonical / GetLongPathNameW 三步各产生一次
            // 文件系统 metadata 往返。对 NAS 上的 UNC 路径，这是三次网络往返，
            // 批量校验 (如 menu handles 数组) 会线性放大为明显卡顿。
            //
            // 对 UNC 而言这三步不提供安全价值:
            //   - canonical 的作用是防符号链接绕过"本机系统盘黑白名单"，
            //     但 ValidatePath 对 UNC 走 ValidateUNCPath 直接放行，
            //     从不触达 IsInBlacklist / IsInWhitelist;
            //     ValidateMediaWriteAccess 的黑名单是本机系统目录，
            //     UNC 路径永远匹配不上。
            //   - GetLongPathNameW 展开 8.3 短名同样只服务于本机黑白名单。
            //   - 本机也无法解析服务器端的 reparse point。
            //
            // 必须保持在设备路径拦截 (\\.\ 与 \\?\) 与 ContainsTraversal 之后:
            // 设备路径前缀同样以 \\ 开头，顺序颠倒会形成绕过通道。
            if (IsUNCPath(path)) {
                resolvedPath = path;
                return true;
            }
            
            // 轨号后缀会让下方 fs::exists 必然 miss 并落入 weakly_canonical 的
            // 逐级前缀探测。剥离后解析的是容器文件本身。
            //
            // 必须保持在 ContainsTraversal 之后: 那是纯字符串检查、不产生 IO，
            // 提前剥离只会缩小被检查的文本范围。
            path = StripSubsongSuffix(path);
            
            // 解析结果按父目录状态戳缓存。
            //
            // 下方 fs::canonical 需要打开内核文件句柄，而父目录的
            // GetFileAttributesExW 是纯 metadata 查询，实测中位数 174.7us
            // 对 5.6us，相差约 31 倍。同一目录下的批量校验 (一张专辑、一个
            // 艺人目录) 因此整批只付一次目录查询，取代逐条句柄创建。
            //
            // 缓存的是本函数最终产出的 resolvedPath (含 8.3 展开)，故命中时
            // 连同下方短名展开一并跳过；两步都只取决于文件系统状态。
            //
            // 判定结论不受影响: 缓存的是解析结果这一纯函数值，允许/拒绝仍由
            // 调用方基于同一个 resolvedPath 判定。取不到状态戳时完全退化为
            // 原有路径，不写缓存。
            const std::optional<uint64_t> parentStamp = QueryParentDirectoryStamp(path);
            if (parentStamp.has_value()) {
                if (auto cached = canonicalCache_.Lookup(path, *parentStamp)) {
                    resolvedPath = std::move(*cached);
                    return true;
                }
            }
            
            // canonical 解析 + 系统盘 8.3 短名展开, 与名单建表共用同一形态化
            // 函数: 判定侧 realPath 与名单条目的形态一致性由该函数单点保证,
            // 逻辑与失败回退语义见其注释。
            resolvedPath = path_canonical::ResolveToCanonicalForm(path, systemDrive_);

            if (parentStamp.has_value()) {
                canonicalCache_.Store(path, resolvedPath, *parentStamp);
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
    
    // 去掉 "|subsong:N" 轨号后缀，返回其所指向的容器文件路径。
    //
    // '|' 是 Win32 文件名保留字符，故带该后缀的字符串永远不可能命名真实文件：
    // 交给 fs::exists 必然 miss，落到 fs::weakly_canonical 的逐级前缀探测
    // （实测中位数 1144us，比命中 canonical 的 163us 贵约 7 倍）。
    //
    // 不改变任何判定结果：后缀只选轨，容器文件与其所有轨道同属一个目录，而黑白
    // 名单、盘符与 UNC 判定全部基于目录前缀。
    static std::wstring StripSubsongSuffix(const std::wstring& path) {
        const size_t pos = path.find(L"|subsong:");
        return pos == std::wstring::npos ? path : path.substr(0, pos);
    }
    
    // 读取 path 所在父目录的当前状态，用作解析结果缓存的有效性判据。
    // 返回 nullopt 表示无法取到判据，此时调用方必须走完整解析且不得写缓存。
    //
    // 取父目录而非文件本身: 目录项的增删改名会更新父目录的最后写入时间，
    // 而"某个名字现在指向什么"正是 canonical 要回答的问题。文件内容变化不
    // 更新目录时间，那也不影响解析结果，故无需为此失效。
    //
    // 一并折入属性字: 目录被换成重解析点时最后写入时间可能被保留，属性字
    // 的变化能补上这一条。
    //
    // 覆盖边界见本类型的缓存注释: 祖先目录链接重指向不被观测。
    static std::optional<uint64_t> QueryParentDirectoryStamp(const std::wstring& path) {
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos || slash == 0) {
            return std::nullopt;
        }
        
        std::wstring parent = path.substr(0, slash);
        // "C:" 是驱动器相对引用而非根目录，必须补上分隔符才指向根。
        if (parent.length() == 2 && parent[1] == L':') {
            parent += L'\\';
        }
        
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!::GetFileAttributesExW(parent.c_str(), GetFileExInfoStandard, &data)) {
            return std::nullopt;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return std::nullopt;
        }
        
        const uint64_t writeTime =
            (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) |
            static_cast<uint64_t>(data.ftLastWriteTime.dwLowDateTime);
        return (writeTime * 1099511628211ULL) ^ static_cast<uint64_t>(data.dwFileAttributes);
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
    
    // 严格写白名单 (profile / temp)。三个写入校验共用同一份判定,
    // 避免各自复制前缀匹配循环后逐渐漂移。
    //
    // 与黑名单/读白名单一致使用分隔符感知的 IsPathPrefixOf: profile 项
    // 不带尾分隔符, 裸 find()==0 前缀匹配会把 "foobar2000evil" 这类
    // 兄弟目录误判为白名单内 (fail-open 方向), 故不可退回裸匹配。
    //
    // 条目已在建表时经 ResolveToCanonicalForm 规范化到与入参 realPath 同一
    // 形态, 这是前缀匹配成立的前提: 条目存原始路径而入参已 canonical 时,
    // 经 junction / 已知文件夹重定向的机器上两者形态错位, 匹配恒 miss
    // (实测反例: profile 目录 junction 到非系统盘后, temp 条目仍是 C:\ 形态
    // 而 realPath 已是 E:\ 形态, temp 写白名单整体静默失效)。
    bool IsInWriteWhitelist(const std::wstring& realPath) {
        std::wstring lowerPath = realPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
        
        for (const auto& allowed : writeAllowedDirs_) {
            std::wstring lowerAllowed = allowed;
            std::transform(lowerAllowed.begin(), lowerAllowed.end(), lowerAllowed.begin(), ::towlower);
            if (IsPathPrefixOf(lowerAllowed, lowerPath)) {
                return true;
            }
        }
        return false;
    }
    
    // 带盘符且盘符不是系统盘。UNC 不算 (无盘符)。
    bool IsOnNonSystemDrive(const std::wstring& realPath) {
        if (IsUNCPath(realPath) || realPath.length() < 2 || realPath[1] != L':') {
            return false;
        }
        return ::towupper(realPath[0]) != systemDrive_;
    }
    
    // 路径是否落在用户配置的媒体库监视目录覆盖范围内。
    //
    // SDK 语义: is_path_addable 回答"当前用户设置是否允许该路径入库",
    // 即该路径是否处于监视目录之下。用户把某目录配成监视目录, 即是显式
    // 把它纳入媒体上下文, 故该判定等价于统一权限架构 §4.2 的第四步。
    //
    // 与"已在库中"是两回事: 刚落盘、尚未扫描入库的文件不在库中, 但已在
    // 监视目录内。缺少本判定会让这类文件无法写标签。
    //
    // 不缓存: 其失效源是监视目录配置变更, 该变更不发 on_items_* 事件,
    // 无事件源可挂, 缓存只能退化为 TTL。详见性能设计 §4.3。
    bool IsInTrustedMediaRoots(const std::wstring& path) {
        try {
            std::string utf8 = pfc::stringcvt::string_utf8_from_wide(path.c_str()).get_ptr();
            return library_manager::get()->is_path_addable(utf8.c_str());
        } catch (...) {
            return false;
        }
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
            
            // 2. 播放列表成员性: 索引查询取代逐条线性扫描
            EnsurePlaylistIndexBuilt();
            const auto member = playlistIndex_.Query(std::string(canonicalPath.c_str()));
            if (member.has_value()) {
                return *member;
            }
            
            // 索引不可用时退回线性扫描, 而非静默判否
            return ScanPlaylistsForPath(canonicalPath.c_str());
        } catch (...) {
            return false;
        }
    }
    
    // 逐条扫描全部播放列表。索引不可用时的回退路径, 也是索引的构建口径来源:
    // 两者都拿 item->get_path() 原值与 g_get_canonical_path 的结果比较,
    // 而 playable_location::case_sensitive 为 true 使 path_compare 退化为
    // strcmp, 故字节精确的集合查询与本函数逐条比较等价。
    bool ScanPlaylistsForPath(const char* canonicalPath) {
        auto pm = playlist_manager::get();
        const t_size plCount = pm->get_playlist_count();
        for (t_size pl = 0; pl < plCount; ++pl) {
            const t_size itemCount = pm->playlist_get_item_count(pl);
            for (t_size i = 0; i < itemCount; ++i) {
                metadb_handle_ptr item;
                if (pm->playlist_get_item_handle(item, pl, i)) {
                    if (metadb::path_compare(canonicalPath, item->get_path()) == 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    // 懒构建播放列表成员索引。跟随 LibraryTreeIndex 的全量失效 + 懒重建范式,
    // 不做增量更新。无条目上限: 旧实现的 50000 项截断会让"确实不在列表中"与
    // "扫描被截断"返回同一个 false, 属正确性缺陷。
    void EnsurePlaylistIndexBuilt() {
        if (playlistIndex_.IsValid()) {
            return;
        }
        auto pm = playlist_manager::get();
        const t_size plCount = pm->get_playlist_count();
        std::vector<std::string> paths;
        for (t_size pl = 0; pl < plCount; ++pl) {
            const t_size itemCount = pm->playlist_get_item_count(pl);
            for (t_size i = 0; i < itemCount; ++i) {
                metadb_handle_ptr item;
                if (pm->playlist_get_item_handle(item, pl, i)) {
                    paths.emplace_back(item->get_path());
                }
            }
        }
        playlistIndex_.Rebuild(std::move(paths));
    }

private:
    fb2k_utils::MediaMembershipIndex playlistIndex_;
    fb2k_utils::PathCanonicalCache canonicalCache_;
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
