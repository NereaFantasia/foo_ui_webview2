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
    // 构造时经 GetFoobarProfilePath 调 core_api (DLL 静态初始化期尚不可用), 必须惰性构造, 不要改成静态成员对象。
    static PathSecurity& Instance();

    // ========================================
    // 主要验证接口
    // ========================================
    
    // 验证路径是否允许访问 (读取)
    bool ValidatePath(const std::wstring& rawPath, std::wstring& errorMsg);
    
    // 验证写入路径 (比读取更严格)
    bool ValidateWritePath(const std::wstring& rawPath, std::wstring& errorMsg);
    
    // 验证媒体访问 (上下文信任)
    //
    // 基础检查失败即拒绝，不再查询监视目录或成员性。读取规则未放行时，仍可按
    // 「拼写集任一命中 且 读侧落点门」检查：受信读取的对象不得是黑名单里的
    // 本机系统目录，除非对象自身拼写受信
    // (播放列表中的 C:\Windows\Media\*.wav 这类已受信对象仍可读)。
    bool ValidateMediaAccess(const std::wstring& rawPath, std::wstring& errorMsg);

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
                                  const std::wstring& contextMediaPath = L"");
    
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
    bool ValidateFileWriteAccess(const std::wstring& rawPath, std::wstring& errorMsg);
    
    // 简化接口
    bool IsPathSafe(const std::wstring& path);
    
    // 作废播放列表成员索引。必须由改变播放列表成员集合的每个 SDK 回调调用,
    // 否则索引会持续返回陈旧的信任判定。下次查询时懒重建。
    void InvalidatePlaylistIndex();

private:
    PathSecurity();
    
    wchar_t systemDrive_ = L'C';
    std::vector<std::wstring> whitelist_;
    std::vector<std::wstring> blacklist_;
    std::vector<std::wstring> writeAllowedDirs_;
    
    // ========================================
    // 初始化
    // ========================================
    
    void InitializeSystemDrive();

    // 名单条目入表口: 统一规范化并丢弃空条目。
    //
    // 规范化依赖 systemDrive_, 故构造函数必须先跑 InitializeSystemDrive
    // 再跑三个建表函数, 该顺序不可调换。
    //
    // 空条目必须丢弃: GetFoobarProfilePath / GetFoobarInstallPath 失败时返回空串,
    // 而 IsPathPrefixOf 对空 prefix 会读 prefix.back() —— 空字符串上是未定义行为,
    // 且长度 0 的前缀在逻辑上匹配一切路径, 是 fail-open 方向。
    void PushNormalized(std::vector<std::wstring>& list, const std::wstring& raw);

    // 所有条目经 PushNormalized 入表: 名单必须存 canonical + 8.3 展开后的形态,
    // 才能与判定侧的 realPath 前缀匹配 (见 ResolveToCanonicalForm 注释)。
    void InitializeWhitelist();
    
    void InitializeBlacklist();

    void InitializeWriteAllowedDirs();
    
    // ========================================
    // 辅助函数
    // ========================================
    
    // 基础路径安全检查 (从 ValidatePath 提取的公共逻辑)
    // 负责: 空路径检查、虚拟协议放行、协议预处理、遍历攻击检测、映射盘改写、
    //       UNC 分类与回环还原、符号链接解析, 产出 ResolvedTarget 供调用方分流
    // 不负责: 黑白名单、系统盘策略、上下文信任 — 这些由调用者自行决定
    bool PassBasicPathSafetyChecks(const std::wstring& rawPath, ResolvedTarget& out, std::wstring& errorMsg);
    
    // UNC 分类与回环改写。Remote → out 直接就绪 (RemoteShare)；Loopback → path
    // 被改写成本地形态, 调用方继续走本地解析; LoopbackUnresolved → false。
    // 非 UNC 输入原样放过 (out 不动)。
    bool ClassifyAndRewriteUnc(std::wstring& path, std::wstring& shareRoot,
                               ResolvedTarget& out, std::wstring& errorMsg);
    
    std::wstring GetFoobarProfilePath();
    
    std::wstring GetFoobarInstallPath();
    
    // 检测不涉及本地文件系统的虚拟/网络协议
    // 这些协议的路径不需要文件系统安全检查（黑白名单、符号链接解析等）
    // 判定契约与实现见 PathProtocolScheme.h (SDK-free, 单元测试直接针对它断言)
    bool IsVirtualOrNetworkProtocol(const std::wstring& path);
    
    // 预处理 FB2K 特殊协议路径
    std::wstring PreprocessProtocolPath(const std::wstring& path);
    
    // 相对分量 (`.` / `..`) 按路径段判定, 不按子串找 `..`: 以句点结尾的曲名接上
    // 扩展名就带 `..` (`Bonus Track..flac`), 是合法文件而不是遍历, 按子串判会把
    // 它们连同整条 MediaRead 链一起拒掉。契约与边界见 PathTraversalSegments.h。
    static bool ContainsTraversal(const std::wstring& path);
    
    // 去掉 "|subsong:N" 轨号后缀，返回其所指向的容器文件路径。
    //
    // '|' 是 Win32 文件名保留字符，故带该后缀的字符串永远不可能命名真实文件：
    // 交给 fs::exists 必然 miss，落到 fs::weakly_canonical 的逐级前缀探测
    // （实测中位数 1144us，比命中 canonical 的 163us 贵约 7 倍）。
    //
    // 不改变任何判定结果：后缀只选轨，容器文件与其所有轨道同属一个目录，而黑白
    // 名单、盘符与 UNC 判定全部基于目录前缀。
    static std::wstring StripSubsongSuffix(const std::wstring& path);
    
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
    static std::optional<uint64_t> QueryParentDirectoryStamp(const std::wstring& path);
    
    bool IsUNCPath(const std::wstring& path);
    
    // 只有流水线分类为真远程的共享才到这里: 真远程放行的实际判据是那次分类,
    // 本函数不再自行判断。设备路径前缀已在流水线前置拦截, 此处检查是冗余护栏。
    bool ValidateUNCPath(const std::wstring& path, std::wstring& errorMsg);
    
    // 安全的目录前缀比较: 要求 path 与 prefix 精确匹配或 path 在 prefix 目录内部
    static bool IsPathPrefixOf(const std::wstring& prefix, const std::wstring& path);

    bool IsInBlacklist(const std::wstring& path);
    
    bool IsInWhitelist(const std::wstring& path);
    
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
    bool IsInWriteWhitelist(const std::wstring& realPath);
    
    // ========================================
    // 信任拼写集与落点门
    // ========================================
    
    // 唯一的「非系统卷」谓词: 本地非系统盘与真远程共享是同一个判据、同一个
    // 删除点, 不得拆成两个谓词或两个开关。虚拟协议产出 {rawPath, Local} 没有
    // 盘符, 由盘符存在性判据排除。
    bool IsOnNonSystemVolume(const ResolvedTarget& t) const;
    
    // 给一条拼写, 生成 fb2k 可能持有的、指向同一对象的全部拼写。只做形态改写
    // (盘符 <-> UNC、回环 <-> 本地), 不解析 reparse point。本函数体内没有文件
    // 系统调用; 经 LettersFor / KnownLoopbackSharesCovering 间接触发的只是
    // 解析器映射表按 TTL 的刷新。
    std::vector<std::wstring> FormRewrites(const std::wstring& spelling);
    
    // 单次校验内的查询记忆; 生命周期只在一条链的函数体内, 不跨调用。落点门查的
    // FormRewrites(t.path) 是信任拼写集的子集, 不记忆化会把 SDK 查询次数翻倍。
    // 键用原拼写, 大小写不折叠: 与 FormRewrites / TrustSpellings 的字节精确去重
    // 同一口径, 也与 fb2k 成员性比较的口径一致。
    struct TrustQueryMemo {
        std::unordered_map<std::wstring, bool> addable;    // is_path_addable
        std::unordered_map<std::wstring, bool> member;     // 库 / 播放列表成员性
    };
    bool MemoAddable(TrustQueryMemo& m, const std::wstring& s);
    bool MemoMember(TrustQueryMemo& m, const std::wstring& s);
    
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
    std::vector<std::wstring> TrustSpellings(const ResolvedTarget& t, const std::wstring& rawPath);
    
    // 拼写集任一命中即为命中。这两个函数只是信任步的「拼写集」半边, 调用处必须
    // 与落点门 (LandingSafeForWrite / LandingSafeForRead) 用 && 连接后才能放行。
    bool IsInTrustedMediaRootsAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings);
    bool IsItemInLibraryOrPlaylistAny(TrustQueryMemo& memo, const std::vector<std::wstring>& spellings);
    // 对象自身拼写受信: 落点对象的拼写集在监视目录或库 / 播放列表中
    bool ObjectSpellingTrusted(TrustQueryMemo& memo, const ResolvedTarget& t);
    // 写侧落点门: 按拼写受信的写入, 其对象自身还必须处于允许落笔的位置
    bool LandingSafeForWrite(TrustQueryMemo& memo, const ResolvedTarget& t);
    // 读侧落点门只守一条: 受信读取的对象不得是黑名单里的本机系统目录
    bool LandingSafeForRead(TrustQueryMemo& memo, const ResolvedTarget& t);
    
    // Read 规则: 真远程放行; 本地按盘符 -> 黑名单 -> 白名单。三条链共用。
    bool ApplyReadRules(const ResolvedTarget& t, std::wstring& errorMsg);
    
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
    bool IsInTrustedMediaRoots(const std::wstring& path);
    
    // 检查文件是否在媒体库或播放列表中 (上下文信任)
    // 修复: 遍历全部播放列表 / 规范化路径比较 / 忽略 subsong index
    bool IsItemInLibraryOrPlaylist(const std::wstring& path);
    
    // 逐条扫描全部播放列表。索引不可用时的回退路径, 也是索引的构建口径来源:
    // 两者都拿 item->get_path() 原值与 g_get_canonical_path 的结果比较,
    // 而 playable_location::case_sensitive 为 true 使 path_compare 退化为
    // strcmp, 故字节精确的集合查询与本函数逐条比较等价。
    bool ScanPlaylistsForPath(const char* canonicalPath);
    
    // 懒构建播放列表成员索引。跟随 LibraryTreeIndex 的全量失效 + 懒重建范式,
    // 不做增量更新。无条目上限: 旧实现的 50000 项截断会让"确实不在列表中"与
    // "扫描被截断"返回同一个 false, 属正确性缺陷。
    void EnsurePlaylistIndexBuilt();

private:
    fb2k_utils::MediaMembershipIndex playlistIndex_;
    fb2k_utils::PathCanonicalCache canonicalCache_;
    fb2k_utils::NetworkShareResolver shareResolver_{fb2k_utils::MakeWin32SystemProbe()};
};

// 便捷函数
bool IsPathSafe(const std::wstring& path);

bool ValidatePath(const std::wstring& path, std::wstring& errorMsg);

bool ValidateWritePath(const std::wstring& path, std::wstring& errorMsg);

bool ValidateMediaAccess(const std::wstring& path, std::wstring& errorMsg);

bool ValidateMediaWriteAccess(const std::wstring& path, std::wstring& errorMsg);
