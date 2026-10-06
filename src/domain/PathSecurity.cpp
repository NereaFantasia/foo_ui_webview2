// ============================================
// PathSecurity.cpp - 统一路径安全验证模块的实现
// 各函数的契约与策略注释在 PathSecurity.h 的声明处。
// ============================================
#include "pch.h"
#include "domain/PathSecurity.h"

namespace fs = std::filesystem;

PathSecurity& PathSecurity::Instance() {
    static PathSecurity instance;
    return instance;
}

bool PathSecurity::ValidatePath(const std::wstring& rawPath, std::wstring& errorMsg) {
    // 虚拟/网络协议早期放行: 不涉及本地文件系统，无需盘符/黑白名单检查
    if (IsVirtualOrNetworkProtocol(rawPath)) return true;
    ResolvedTarget t;
    if (!PassBasicPathSafetyChecks(rawPath, t, errorMsg)) return false;
    try {
        return ApplyReadRules(t, errorMsg);
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
}

bool PathSecurity::ValidateWritePath(const std::wstring& rawPath, std::wstring& errorMsg) {
    ResolvedTarget t;
    if (!PassBasicPathSafetyChecks(rawPath, t, errorMsg)) return false;
    // 虚拟协议跳过读取规则，但仍须通过写白名单。远程共享读取放行并不代表
    // 可写；其 UNC 路径也必须通过后面的写白名单检查。
    if (!IsVirtualOrNetworkProtocol(rawPath)) {
        try {
            if (!ApplyReadRules(t, errorMsg)) return false;
        } catch (const std::exception&) {
            errorMsg = L"Path validation error";
            return false;
        }
    }
    // 复用同一个 t: 流水线产出与写白名单条目经同一 ResolveToCanonicalForm
    // 形态化，两侧形态一致，无需再次 canonical，避免映射盘与白名单使用不同形态。
    if (IsInWriteWhitelist(t.path)) return true;
    errorMsg = L"Write access denied: only profile and temp directories allowed";
    return false;
}

bool PathSecurity::ValidateMediaAccess(const std::wstring& rawPath, std::wstring& errorMsg) {
    if (IsVirtualOrNetworkProtocol(rawPath)) return true;
    ResolvedTarget t;
    if (!PassBasicPathSafetyChecks(rawPath, t, errorMsg)) return false;   // 基础检查失败 → 硬拒绝, 不再回退
    try {
        if (ApplyReadRules(t, errorMsg)) return true;
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
    try {
        TrustQueryMemo memo;
        const auto s = TrustSpellings(t, rawPath);
        if (IsInTrustedMediaRootsAny(memo, s) && LandingSafeForRead(memo, t)) { errorMsg.clear(); return true; }
        if (IsItemInLibraryOrPlaylistAny(memo, s) && LandingSafeForRead(memo, t)) { errorMsg.clear(); return true; }
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
    return false;
}

bool PathSecurity::ValidateMediaWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg,
                                            const std::wstring& contextMediaPath) {
    ResolvedTarget t;
    if (!PassBasicPathSafetyChecks(rawPath, t, errorMsg)) return false;
    // 黑名单必须最先判, 且判的是解析后的 t.path: 经 junction 指向系统目录的
    // 路径在此被拦下, 后续各步便无需各自防御该绕过通道。
    if (IsInBlacklist(t.path)) {
        errorMsg = L"Write access denied: protected system path";
        return false;
    }
    // 1. 严格写白名单 (profile/temp)
    if (IsInWriteWhitelist(t.path)) return true;
    try {
        TrustQueryMemo memo;
        const auto s = TrustSpellings(t, rawPath);
        // 2. 受信任媒体上下文 (媒体库/播放列表)
        if (IsItemInLibraryOrPlaylistAny(memo, s) && LandingSafeForWrite(memo, t)) return true;
        // 3. 媒体库监视目录 — 覆盖已落盘但尚未扫描入库的文件
        if (IsInTrustedMediaRootsAny(memo, s) && LandingSafeForWrite(memo, t)) return true;
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
    // 4. 同目录派生文件信任: 写入路径与受信上下文音频文件在同一父目录。
    // 伴生文件 (.lrc 等) 本身不在库中, 故须以源音频的上下文判定。
    if (!contextMediaPath.empty()) {
        // 伴生规则原样 (fs::equivalent 按对象比; contextMediaPath 是 fb2k 交来的拼写)
        try {
            auto writtenDir = std::filesystem::path(t.path).parent_path();
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

bool PathSecurity::ValidateFileWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg) {
    ResolvedTarget t;
    if (!PassBasicPathSafetyChecks(rawPath, t, errorMsg)) return false;
    if (IsInBlacklist(t.path)) {
        errorMsg = L"Write access denied: protected system path";
        return false;
    }
    if (IsInWriteWhitelist(t.path)) return true;
    try {
        TrustQueryMemo memo;
        const auto s = TrustSpellings(t, rawPath);
        // 信任步的拼写集含调用方拼写与 canonical 之后的对象拼写, 但放行还要过
        // 落点门: 判定落点必须等于实际写入落点。只按调用方拼写判定时, 监视目录
        // 内的重解析点 (junction/symlink) 能把写入重定向到监视目录之外、又不在
        // 黑名单内的位置 (如启动项目录), 而该位置直接传入时是被拒绝的; 落点门
        // 按对象自身 (t.path) 是否位于写白名单、非系统卷或自身拼写受信的位置
        // 把这条通道关上。与下方 8.3 展开处"判据必须取 canonical 之后"是同一
        // 道理。t.path 对本步的两类正当输入均可用: 新建路径走 weakly_canonical
        // 逐级解析、不存在的尾段原样保留; 真远程共享跳过解析、原样返回。
        //
        // 落点门在本链的实际效果按对象所在卷分两种: 非系统卷对象的落点门恒真
        // (非系统卷子句), 第一步与紧随其后的直通等价; 系统盘对象的落点门只剩
        // 「写白名单 或 对象自身拼写受信」, 所以本链对系统盘对象实际只认对象
        // 自身拼写, 调用方拼写在此链只影响归因到哪一步。这是有意收紧: 经
        // junction 拼写入库、对象落在系统盘的曲目不再放行。
        if (IsInTrustedMediaRootsAny(memo, s) && LandingSafeForWrite(memo, t)) return true;
        if (IsOnNonSystemVolume(t)) return true;
        if (IsItemInLibraryOrPlaylistAny(memo, s) && LandingSafeForWrite(memo, t)) return true;
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
    errorMsg = L"Write access denied: system drive path is not in trusted media context";
    return false;
}

bool PathSecurity::IsPathSafe(const std::wstring& path) {
    std::wstring errorMsg;
    return ValidatePath(path, errorMsg);
}

void PathSecurity::InvalidatePlaylistIndex() {
    playlistIndex_.Invalidate();
}

PathSecurity::PathSecurity() {
    InitializeSystemDrive();
    InitializeWhitelist();
    InitializeBlacklist();
    InitializeWriteAllowedDirs();
}

void PathSecurity::InitializeSystemDrive() {
    wchar_t winDir[MAX_PATH];
    if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
        systemDrive_ = ::towupper(winDir[0]);
    }
}

void PathSecurity::PushNormalized(std::vector<std::wstring>& list, const std::wstring& raw) {
    if (raw.empty()) return;
    std::wstring normalized = path_canonical::ResolveToCanonicalForm(raw, systemDrive_);
    if (normalized.empty()) return;
    list.push_back(std::move(normalized));
}

void PathSecurity::InitializeWhitelist() {
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

void PathSecurity::InitializeBlacklist() {
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

void PathSecurity::InitializeWriteAllowedDirs() {
    writeAllowedDirs_.clear();

    // Profile 目录
    PushNormalized(writeAllowedDirs_, GetFoobarProfilePath());

    // Temp 目录
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath) > 0) {
        PushNormalized(writeAllowedDirs_, tempPath);
    }
}

bool PathSecurity::PassBasicPathSafetyChecks(const std::wstring& rawPath, ResolvedTarget& out, std::wstring& errorMsg) {
    try {
        // 出参先清空: 下方以 out.kind == RemoteShare 作早退条件, 安全层的早退
        // 判断不能依赖「调用方传入的是新构造对象」这一函数体外的不变量。
        out = {};
        if (rawPath.empty()) {
            errorMsg = L"Empty path";
            return false;
        }
        
        // 虚拟/网络协议早期放行: 不涉及本地文件系统，无需路径安全检查
        if (IsVirtualOrNetworkProtocol(rawPath)) {
            out = {rawPath, TargetKind::Local, L"", rawPath};
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
        
        // 映射盘先改写成 UNC。查表本身是纯字符串比对, 不产生 IO; 但映射表
        // 从未加载或过期后的首次调用会刷新它: 枚举映射盘并对每个目标共享探测
        // 一次共享根 (离线 NAS 为一次 SMB 超时), 任何盘符路径 (含 C:\、E:\
        // 这类本地路径) 都可能触发。代价登记在 NetworkShareResolver.h 头注释;
        // 本层不改变这一刷新策略；后台刷新或限定触发入口属于缓存策略调整。
        //
        // 只改写绝对形态 (X:\ 与 X:/); 盘符相对拼写 (X:dir\f、裸 X:) 在 Win32
        // 语义下指 X: 当前目录之下, 交给下方 canonical 按当前目录展开, 避免
        // 判定对象与 OS 实际打开的对象脱节。改写后真远程的 Z:\ 与 UNC 一样
        // 享受下方早退; 未改写的 (盘符相对拼写、刷新窗口内的新映射、查询失败)
        // 落入本地解析, canonical 若产出 UNC 形态, 由末尾的回环归一化处理。
        if (path.length() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
            if (auto share = shareResolver_.ShareRootOfLetter(path[0])) {
                path = fb2k_utils::JoinLocalRoot(*share, path.substr(2));
            }
        }
        
        // 真远程共享: 跳过文件系统解析 (性能关键路径)
        //
        // 下方 canonical 解析对 NAS 上的路径意味着多次文件系统 metadata
        // 网络往返, 批量校验 (如 menu handles 数组) 会线性放大为明显卡顿。
        // Remote 判定按共享缓存 60 秒；新拼写或并发缓存未命中仍可能重复探测。
        //
        // 早退只对真远程共享成立: 对象不在本机, 本机系统目录的黑白名单对它
        // 无意义, 本机也无法解析服务端的 reparse point。回环共享在此被还原
        // 成本地路径, 继续进入下方完整的本地解析, 黑白名单照常生效。
        //
        // 必须保持在设备路径拦截 (\\.\ 与 \\?\) 与 ContainsTraversal 之后:
        // 设备路径前缀同样以 \\ 开头，顺序颠倒会形成绕过通道。
        std::wstring shareRoot;
        std::wstring callerForm;
        if (!ClassifyAndRewriteUnc(path, shareRoot, out, errorMsg)) {
            return false;
        }
        if (out.kind == TargetKind::RemoteShare) {
            return true;
        }
        
        // 轨号后缀会让下方 fs::exists 必然 miss 并落入 weakly_canonical 的
        // 逐级前缀探测。剥离后解析的是容器文件本身。
        //
        // 必须保持在 ContainsTraversal 之后: 那是纯字符串检查、不产生 IO，
        // 提前剥离只会缩小被检查的文本范围。
        path = StripSubsongSuffix(path);
        callerForm = path;
        
        // 解析结果按父目录状态戳缓存。
        //
        // 下方 fs::canonical 需要打开内核文件句柄，而父目录的
        // GetFileAttributesExW 是纯 metadata 查询，实测中位数 174.7us
        // 对 5.6us，相差约 31 倍。同一目录下的批量校验 (一张专辑、一个
        // 艺人目录) 因此整批只付一次目录查询，取代逐条句柄创建。
        //
        // 缓存的是 canonical 产出 (含 8.3 展开)，故命中时连同短名展开一并
        // 跳过；两步都只取决于文件系统状态。命中分支不直接返回: 首次解析
        // 可能把 UNC 形态存进缓存, 命中的产出同样要经过下方的回环归一化。
        //
        // 判定结论不受影响: 缓存的是解析结果这一纯函数值，允许/拒绝仍由
        // 调用方基于同一个产出判定。取不到状态戳时完全退化为原有路径，
        // 不写缓存。
        std::wstring resolved;
        const std::optional<uint64_t> parentStamp = QueryParentDirectoryStamp(path);
        if (parentStamp.has_value()) {
            if (auto cached = canonicalCache_.Lookup(path, *parentStamp)) {
                resolved = std::move(*cached);
            }
        }
        if (resolved.empty()) {
            // canonical 解析 + 系统盘 8.3 短名展开, 与名单建表共用同一形态化
            // 函数: 判定侧 t.path 与名单条目的形态一致性由该函数单点保证,
            // 逻辑与失败回退语义见其注释。
            resolved = path_canonical::ResolveToCanonicalForm(path, systemDrive_);
            if (parentStamp.has_value()) {
                canonicalCache_.Store(path, resolved, *parentStamp);  // NOLINT(readability-suspicious-call-argument): 实参顺序已按 Store(key, resolvedPath, stamp) 核对
            }
        }
        
        // 回环归一化 (至多一次): canonical 仍产出 UNC 时 (映射表尚未刷新的映射
        // 盘、subst 到 UNC 等) 再分类一次。第二轮为真远程 → 按远程返回, 但
        // callerForm 保留第一轮取值 (那才是 fb2k 可能持有的调用方拼写, 如
        // Z:\...); 第二轮为回环 → 还原后再 canonical 一次; 之后仍是 UNC 一律
        // 拒绝, 不得当远程放行, 也不得把 UNC 字符串包成本地路径交给下游。
        if (IsUNCPath(resolved)) {
            ResolvedTarget second;
            std::wstring secondShare;
            std::wstring rewritten = resolved;
            if (!ClassifyAndRewriteUnc(rewritten, secondShare, second, errorMsg)) {
                return false;
            }
            if (second.kind == TargetKind::RemoteShare) {
                second.callerForm = callerForm;
                out = std::move(second);
                return true;
            }
            resolved = path_canonical::ResolveToCanonicalForm(rewritten, systemDrive_);
            if (IsUNCPath(resolved)) {
                errorMsg = L"Path resolution did not converge";
                return false;
            }
            if (shareRoot.empty()) shareRoot = secondShare;
        }
        
        out = {std::move(resolved), TargetKind::Local, std::move(shareRoot), std::move(callerForm)};
        return true;
    } catch (const std::exception&) {
        errorMsg = L"Path validation error";
        return false;
    }
}

bool PathSecurity::ClassifyAndRewriteUnc(std::wstring& path, std::wstring& shareRoot,
                                         ResolvedTarget& out, std::wstring& errorMsg) {
    if (!IsUNCPath(path)) return true;
    const auto split = fb2k_utils::SplitUncShareRoot(path);
    if (!split) {
        errorMsg = L"Invalid path format";
        return false;
    }
    shareRoot = split->first;
    const auto info = shareResolver_.Classify(shareRoot);
    switch (info.kind) {
        case fb2k_utils::ShareKind::Remote:
            out = {path, TargetKind::RemoteShare, shareRoot, path};
            return true;
        case fb2k_utils::ShareKind::Loopback:
            path = fb2k_utils::JoinLocalRoot(info.localRoot, split->second);
            return true;
        case fb2k_utils::ShareKind::LoopbackUnresolved:
            errorMsg = L"Cannot resolve loopback share to a local path";
            return false;
    }
    errorMsg = L"Path validation error";
    return false;
}

std::wstring PathSecurity::GetFoobarProfilePath() {
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

std::wstring PathSecurity::GetFoobarInstallPath() {
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

bool PathSecurity::IsVirtualOrNetworkProtocol(const std::wstring& path) {
    return path_protocol::IsVirtualOrNetworkProtocol(path);
}

std::wstring PathSecurity::PreprocessProtocolPath(const std::wstring& path) {
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

bool PathSecurity::ContainsTraversal(const std::wstring& path) {
    return path_traversal::ContainsTraversalSegment(path);
}

std::wstring PathSecurity::StripSubsongSuffix(const std::wstring& path) {
    const size_t pos = path.find(L"|subsong:");
    return pos == std::wstring::npos ? path : path.substr(0, pos);
}

std::optional<uint64_t> PathSecurity::QueryParentDirectoryStamp(const std::wstring& path) {
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

bool PathSecurity::IsUNCPath(const std::wstring& path) {
    return path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

bool PathSecurity::ValidateUNCPath(const std::wstring& path, std::wstring& errorMsg) {
    // 禁止设备路径
    if (path.find(L"\\\\.\\") == 0 || path.find(L"\\\\?\\") == 0) {
        errorMsg = L"Device paths not allowed";
        return false;
    }
    
    // UNC 网络路径允许 (NAS 支持)
    return true;
}

bool PathSecurity::IsPathPrefixOf(const std::wstring& prefix, const std::wstring& path) {
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

bool PathSecurity::IsInBlacklist(const std::wstring& path) {
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

bool PathSecurity::IsInWhitelist(const std::wstring& path) {
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

bool PathSecurity::IsInWriteWhitelist(const std::wstring& realPath) {
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

bool PathSecurity::IsOnNonSystemVolume(const ResolvedTarget& t) const {
    if (t.kind == TargetKind::RemoteShare) return true;
    if (t.path.length() < 2 || t.path[1] != L':') return false;
    return ::towupper(t.path[0]) != systemDrive_;
}

std::vector<std::wstring> PathSecurity::FormRewrites(const std::wstring& spelling) {
    std::vector<std::wstring> out;
    // 字节精确去重: fb2k 的成员性比较是字节精确的 (path_compare 退化为
    // strcmp), 大小写不同的两条拼写对它是两个不同的查询键, 都要保留。
    auto push = [&out](std::wstring s) {
        for (const auto& e : out) if (e == s) return;
        out.push_back(std::move(s));
    };
    push(spelling);
    if (IsUNCPath(spelling)) {
        if (const auto split = fb2k_utils::SplitUncShareRoot(spelling)) {
            // 余部为空时盘符形态取卷根 L:\, 不生成裸 L: (那是盘符相对拼写,
            // 流水线不改写它, 这里也不该产出它)。
            for (wchar_t letter : shareResolver_.LettersFor(split->first)) {
                push(std::wstring(1, letter) + L":" + (split->second.empty() ? L"\\" : split->second));
            }
        }
        return out;
    }
    if (spelling.length() >= 2 && spelling[1] == L':') {
        for (const auto& [shareRoot, localRoot] : shareResolver_.KnownLoopbackSharesCovering(spelling)) {
            // rest 是 spelling 去掉 localRoot 后的余部, 规范成以 \ 开头 (或为空):
            //   localRoot=E:\   spelling=E:\OST\x → rest=\OST\x
            //   localRoot=E:\OST spelling=E:\OST\x → rest=\x
            //   localRoot=E:\OST spelling=E:\OST   → rest=""
            // rest 为空时 UNC 形态就是共享根本身, 盘符形态取卷根 L:\ (同上,
            // 不生成裸 L:)。
            std::wstring rest = spelling.substr(localRoot.size());
            if (!localRoot.empty() && (localRoot.back() == L'\\' || localRoot.back() == L'/')) {
                rest.insert(0, 1, L'\\');
            }
            push(shareRoot + rest);
            for (wchar_t letter : shareResolver_.LettersFor(shareRoot)) {
                push(std::wstring(1, letter) + L":" + (rest.empty() ? L"\\" : rest));
            }
        }
    }
    return out;
}

bool PathSecurity::MemoAddable(TrustQueryMemo& m, const std::wstring& s) {
    auto it = m.addable.find(s);
    if (it == m.addable.end()) it = m.addable.emplace(s, IsInTrustedMediaRoots(s)).first;
    return it->second;
}

bool PathSecurity::MemoMember(TrustQueryMemo& m, const std::wstring& s) {
    auto it = m.member.find(s);
    if (it == m.member.end()) it = m.member.emplace(s, IsItemInLibraryOrPlaylist(s)).first;
    return it->second;
}

std::vector<std::wstring> PathSecurity::TrustSpellings(const ResolvedTarget& t, const std::wstring& rawPath) {
    std::vector<std::wstring> s;
    auto push = [&s](std::wstring p) {
        for (const auto& e : s) if (e == p) return;
        s.push_back(std::move(p));
    };
    push(rawPath);
    for (auto& p : FormRewrites(t.callerForm)) push(std::move(p));
    for (auto& p : FormRewrites(t.path)) push(std::move(p));
    return s;
}

bool PathSecurity::IsInTrustedMediaRootsAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings) {
    for (const auto& s : spellings) if (MemoAddable(memo, s)) return true;
    return false;
}

bool PathSecurity::IsItemInLibraryOrPlaylistAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings) {
    for (const auto& s : spellings) if (MemoMember(memo, s)) return true;
    return false;
}

bool PathSecurity::ObjectSpellingTrusted(TrustQueryMemo& memo, const ResolvedTarget& t) {
    const auto forms = FormRewrites(t.path);
    return IsInTrustedMediaRootsAny(memo, forms) || IsItemInLibraryOrPlaylistAny(memo, forms);
}

bool PathSecurity::LandingSafeForWrite(TrustQueryMemo& memo, const ResolvedTarget& t) {
    return IsInWriteWhitelist(t.path) || IsOnNonSystemVolume(t) || ObjectSpellingTrusted(memo, t);
}

bool PathSecurity::LandingSafeForRead(TrustQueryMemo& memo, const ResolvedTarget& t) {
    return !IsInBlacklist(t.path) || ObjectSpellingTrusted(memo, t);
}

bool PathSecurity::ApplyReadRules(const ResolvedTarget& t, std::wstring& errorMsg) {
    if (t.kind == TargetKind::RemoteShare) {
        return ValidateUNCPath(t.path, errorMsg);
    }
    if (t.path.length() < 2 || t.path[1] != L':') {
        errorMsg = L"Invalid path format";
        return false;
    }
    const wchar_t drive = ::towupper(t.path[0]);
    if (drive != systemDrive_) return true;
    if (IsInBlacklist(t.path)) {
        errorMsg = L"Access denied: protected system path";
        return false;
    }
    if (IsInWhitelist(t.path)) return true;
    errorMsg = L"Access denied: system drive path not in whitelist";
    return false;
}

bool PathSecurity::IsInTrustedMediaRoots(const std::wstring& path) {
    try {
        std::string utf8 = pfc::stringcvt::string_utf8_from_wide(path.c_str()).get_ptr();
        return library_manager::get()->is_path_addable(utf8.c_str());
    } catch (...) {
        return false;
    }
}

bool PathSecurity::IsItemInLibraryOrPlaylist(const std::wstring& path) {
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

bool PathSecurity::ScanPlaylistsForPath(const char* canonicalPath) {
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

void PathSecurity::EnsurePlaylistIndexBuilt() {
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

bool IsPathSafe(const std::wstring& path) {
    return PathSecurity::Instance().IsPathSafe(path);
}

bool ValidatePath(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidatePath(path, errorMsg);
}

bool ValidateWritePath(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateWritePath(path, errorMsg);
}

bool ValidateMediaAccess(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateMediaAccess(path, errorMsg);
}

bool ValidateMediaWriteAccess(const std::wstring& path, std::wstring& errorMsg) {
    return PathSecurity::Instance().ValidateMediaWriteAccess(path, errorMsg);
}
