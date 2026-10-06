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
//   - 网络路径: 映射盘先改写为 UNC; 回环共享 (指向本机对象的 \\Server\Share)
//     按其指向的本地路径判定; 真远程共享视同非系统盘
//   - 映射表刷新可能在任意绝对盘符路径的校验中触发一轮共享根探测
//     (代价登记在 NetworkShareResolver.h)
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
#include <unordered_map>
#include <ShlObj.h>
#include <foobar2000/SDK/foobar2000.h>
#include "utils/MediaMembershipIndex.h"
#include "utils/PathCanonicalCache.h"
#include "utils/PathCanonicalForm.h"
#include "utils/PathProtocolScheme.h"
#include "utils/PathTraversalSegments.h"
#include "utils/NetworkShareResolver.h"

namespace fs = std::filesystem;

// 路径解析流水线的产出。Local 与 RemoteShare 由调用方分流: 前者进入盘符 /
// 黑白名单判定, 后者的对象不在本机, 黑白名单对它无意义。
enum class TargetKind { Local, RemoteShare };
struct ResolvedTarget {
    std::wstring path;        // Local: canonical 形态; RemoteShare: \\host\share\rest 原样
                              // (远程早退发生在轨号剥离之前, 故保留 |subsong:N 后缀, 与既有
                              // 行为一致, 既有断言依赖此形态)
    TargetKind kind = TargetKind::Local;
    std::wstring shareRoot;   // 途经的 \\host\share; Local 未经回环时为空。两轮都途经共享时:
                              // 第二轮为 Remote 取对象所在的第二轮共享; 第二轮为 Loopback 保留
                              // 第一轮的入口共享 (第一轮未途经共享时才取第二轮的)
    std::wstring callerForm;  // 第一轮进入 canonical 之前的拼写 (协议 / 轨号剥离、映射盘与
                              // 回环改写之后), 即 fb2k 可能持有的调用方拼写; RemoteShare
                              // 未经回环归一化时与 path 相同 (同样带 |subsong:N 后缀); 回环归一化的
                              // 第二轮不覆盖
};

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
    
    // 验证写入路径 (比读取更严格)
    bool ValidateWritePath(const std::wstring& rawPath, std::wstring& errorMsg) {
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
    
    // 验证媒体访问 (上下文信任)
    //
    // 基础检查失败即拒绝，不再查询监视目录或成员性。读取规则未放行时，仍可按
    // 「拼写集任一命中 且 读侧落点门」检查：受信读取的对象不得是黑名单里的
    // 本机系统目录，除非对象自身拼写受信
    // (播放列表中的 C:\Windows\Media\*.wav 这类已受信对象仍可读)。
    bool ValidateMediaAccess(const std::wstring& rawPath, std::wstring& errorMsg) {
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

    // 验证媒体写入: 只允许写进用户已显式纳入媒体上下文的位置。
    //
    // 比 MediaRead 更严格 —— 读侧的"非系统盘默认放行"不得继承到写侧,
    // 否则等于"只要在 D:/E: 上就能改任意音频文件"。
    //
    // 允许集合恰为四类: 严格写白名单 (profile/temp)、媒体库或播放列表中的
    // 文件、媒体库监视目录覆盖范围内的文件、以及与受信上下文音频同目录的
    // 派生伴生文件。其余一律拒绝。
    //
    // 成员性与监视目录两步按「信任拼写集任一命中 且 写侧落点门」判定: 拼写集
    // 是调用方拼写与对象拼写的形态改写并集, 落点门要求对象自身位于写白名单、
    // 非系统卷或自身拼写受信的位置。只按调用方拼写放行时, 监视目录内的
    // junction 能把写入洗到系统盘上直接传入即被拒的目标, 落点门把它关上。
    bool ValidateMediaWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg,
                                  const std::wstring& contextMediaPath = L"") {
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
    
    // 验证通用文件写入 (file.* 端点)。
    //
    // 与 ValidateMediaWriteAccess 的差别: 本函数多"非系统卷直通" (本地非系统盘
    // 与真远程共享同一谓词)、少同目录伴生文件信任, 且监视目录步排在直通之前。
    // file.mkdir 建的新目录、file.write 建的新文件都不可能预先存在于媒体库或
    // 播放列表中, 只靠这两个信任源会使整族端点失效。
    //
    // 该直通是过渡策略, 不是本函数的目标态: 目标态是把
    // file.write / delete / mkdir / copy.destination / move / rename 归入
    // 严格写白名单 (profile/temp)。改归之后, 非系统盘任意路径的写入与删除
    // 将被拒绝 —— 那是公开 SDK 文档当前明确承诺的行为, 属破坏性变更, 须随
    // 版本策略单独决策。此处先把它从媒体写入语义中摘出, 使其成为显式登记的
    // 独立策略, 而非搭媒体写入的便车。
    //
    // 监视目录信任源排在该直通**之前**是为了决定步稳定, 不是为了功能存续:
    // 本链除黑名单外每步都只在命中时 return true, 没有一步在未命中时中断,
    // 故位置不改变放行集合 —— 系统盘监视目录内的路径排在直通之后同样会命中
    // (IsOnNonSystemVolume 对系统盘为 false, 遮不住它)。排在之前的收益是删
    // 直通后由监视目录接管决定步, 判定归因与顺序用例断言都不变。
    bool ValidateFileWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg) {
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
    // 负责: 空路径检查、虚拟协议放行、协议预处理、遍历攻击检测、映射盘改写、
    //       UNC 分类与回环还原、符号链接解析, 产出 ResolvedTarget 供调用方分流
    // 不负责: 黑白名单、系统盘策略、上下文信任 — 这些由调用者自行决定
    bool PassBasicPathSafetyChecks(const std::wstring& rawPath, ResolvedTarget& out, std::wstring& errorMsg) {
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
    
    // UNC 分类与回环改写。Remote → out 直接就绪 (RemoteShare)；Loopback → path
    // 被改写成本地形态, 调用方继续走本地解析; LoopbackUnresolved → false。
    // 非 UNC 输入原样放过 (out 不动)。
    bool ClassifyAndRewriteUnc(std::wstring& path, std::wstring& shareRoot,
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
    // 判定契约与实现见 PathProtocolScheme.h (SDK-free, 单元测试直接针对它断言)
    bool IsVirtualOrNetworkProtocol(const std::wstring& path) {
        return path_protocol::IsVirtualOrNetworkProtocol(path);
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
    
    // 相对分量 (`.` / `..`) 按路径段判定, 不按子串找 `..`: 以句点结尾的曲名接上
    // 扩展名就带 `..` (`Bonus Track..flac`), 是合法文件而不是遍历, 按子串判会把
    // 它们连同整条 MediaRead 链一起拒掉。契约与边界见 PathTraversalSegments.h。
    static bool ContainsTraversal(const std::wstring& path) {
        return path_traversal::ContainsTraversalSegment(path);
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
    
    // 只有流水线分类为真远程的共享才到这里: 真远程放行的实际判据是那次分类,
    // 本函数不再自行判断。设备路径前缀已在流水线前置拦截, 此处检查是冗余护栏。
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
    
    // ========================================
    // 信任拼写集与落点门
    // ========================================
    
    // 唯一的「非系统卷」谓词: 本地非系统盘与真远程共享是同一个判据、同一个
    // 删除点, 不得拆成两个谓词或两个开关。虚拟协议产出 {rawPath, Local} 没有
    // 盘符, 由盘符存在性判据排除。
    bool IsOnNonSystemVolume(const ResolvedTarget& t) const {
        if (t.kind == TargetKind::RemoteShare) return true;
        if (t.path.length() < 2 || t.path[1] != L':') return false;
        return ::towupper(t.path[0]) != systemDrive_;
    }
    
    // 给一条拼写, 生成 fb2k 可能持有的、指向同一对象的全部拼写。只做形态改写
    // (盘符 <-> UNC、回环 <-> 本地), 不解析 reparse point。本函数体内没有文件
    // 系统调用; 经 LettersFor / KnownLoopbackSharesCovering 间接触发的只是
    // 解析器映射表按 TTL 的刷新。
    std::vector<std::wstring> FormRewrites(const std::wstring& spelling) {
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
    
    // 单次校验内的查询记忆; 生命周期只在一条链的函数体内, 不跨调用。落点门查的
    // FormRewrites(t.path) 是信任拼写集的子集, 不记忆化会把 SDK 查询次数翻倍。
    // 键用原拼写, 大小写不折叠: 与 FormRewrites / TrustSpellings 的字节精确去重
    // 同一口径, 也与 fb2k 成员性比较的口径一致。
    struct TrustQueryMemo {
        std::unordered_map<std::wstring, bool> addable;    // is_path_addable
        std::unordered_map<std::wstring, bool> member;     // 库 / 播放列表成员性
    };
    bool MemoAddable(TrustQueryMemo& m, const std::wstring& s) {
        auto it = m.addable.find(s);
        if (it == m.addable.end()) it = m.addable.emplace(s, IsInTrustedMediaRoots(s)).first;
        return it->second;
    }
    bool MemoMember(TrustQueryMemo& m, const std::wstring& s) {
        auto it = m.member.find(s);
        if (it == m.member.end()) it = m.member.emplace(s, IsItemInLibraryOrPlaylist(s)).first;
        return it->second;
    }
    
    // 信任拼写集, 三部分取并集 (字节精确去重), 都不能少:
    //   - 调用方原始拼写 rawPath 原样。它与旧实现查询的字符串完全一致, 保证新集合
    //     是旧实现查询集的超集。单靠形态改写会漏掉它: 解析器对同一共享的多种大小
    //     写拼写只保留首见的一条, 调用方传 \\LOCALHOST\e$\... 而表里存的是
    //     \\localhost\E$ 时, 改写只产出后者。
    //   - callerForm (fb2k 可能持有的、经 junction 入库时存的那条) 的形态改写, 让
    //     按 junction 拼写入库的曲目仍能命中成员性。
    //   - canonical 之后的对象拼写 t.path 的形态改写, 让映射盘 / UNC 注册的库根
    //     经形态改写命中。
    // rawPath 与其他成员地位相同: 命中后同样只经落点门放行。
    std::vector<std::wstring> TrustSpellings(const ResolvedTarget& t, const std::wstring& rawPath) {
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
    
    // 拼写集任一命中即为命中。这两个函数只是信任步的「拼写集」半边, 调用处必须
    // 与落点门 (LandingSafeForWrite / LandingSafeForRead) 用 && 连接后才能放行。
    bool IsInTrustedMediaRootsAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings) {
        for (const auto& s : spellings) if (MemoAddable(memo, s)) return true;
        return false;
    }
    bool IsItemInLibraryOrPlaylistAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings) {
        for (const auto& s : spellings) if (MemoMember(memo, s)) return true;
        return false;
    }
    // 对象自身拼写受信: 落点对象的拼写集在监视目录或库 / 播放列表中
    bool ObjectSpellingTrusted(TrustQueryMemo& memo, const ResolvedTarget& t) {
        const auto forms = FormRewrites(t.path);
        return IsInTrustedMediaRootsAny(memo, forms) || IsItemInLibraryOrPlaylistAny(memo, forms);
    }
    // 写侧落点门: 按拼写受信的写入, 其对象自身还必须处于允许落笔的位置
    bool LandingSafeForWrite(TrustQueryMemo& memo, const ResolvedTarget& t) {
        return IsInWriteWhitelist(t.path) || IsOnNonSystemVolume(t) || ObjectSpellingTrusted(memo, t);
    }
    // 读侧落点门只守一条: 受信读取的对象不得是黑名单里的本机系统目录
    bool LandingSafeForRead(TrustQueryMemo& memo, const ResolvedTarget& t) {
        return !IsInBlacklist(t.path) || ObjectSpellingTrusted(memo, t);
    }
    
    // Read 规则: 真远程放行; 本地按盘符 -> 黑名单 -> 白名单。三条链共用。
    bool ApplyReadRules(const ResolvedTarget& t, std::wstring& errorMsg) {
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
    
    // 路径是否落在用户配置的媒体库监视目录覆盖范围内。
    //
    // SDK 语义: is_path_addable 回答"当前用户设置是否允许该路径入库",
    // 即该路径是否处于监视目录之下。用户把某目录配成监视目录, 即是显式
    // 把它纳入媒体上下文, 故该判定为真即视为路径处于媒体上下文中。
    //
    // 与"已在库中"是两回事: 刚落盘、尚未扫描入库的文件不在库中, 但已在
    // 监视目录内。缺少本判定会让这类文件无法写标签。
    //
    // 不缓存: 其失效源是监视目录配置变更, 该变更不发 on_items_* 事件,
    // 无事件源可挂, 缓存只能退化为 TTL。
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
    fb2k_utils::NetworkShareResolver shareResolver_{fb2k_utils::MakeWin32SystemProbe()};
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
