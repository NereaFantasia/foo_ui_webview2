#pragma once
// ============================================
// NetworkShareResolver.h - 网络共享分类 (回环 / 远程) 与映射盘表的单点实现
// ============================================
//
// PathSecurity 需要回答: 一条 \\host\share\... 路径究竟指向别的机器, 还是经
// SMB 绕了一圈又落回本机磁盘 (\\localhost\E$、\\127.0.0.1\E$ 这类回环共享)。
// 前者按远程共享判定, 后者必须等价于对应的本地路径判定, 否则同一个文件换一种
// 拼写就能拿到不同的权限结论。本头负责这一分类, 以及映射盘符 (Z: 对应哪个
// \\host\share) 的表。
//
// 本头只依赖 STL 与 Win32，系统调用经 ISystemProbe 注入。测试无需链接 fb2k SDK
// 或连接真实 SMB 共享，即可验证同一个分类实现及其缓存过期行为。
//
// 不用 NetShareEnum 取共享的本地路径: 那需要 level 2 信息, 而 level 2 要求管理员
// 权限。foobar2000 非提权运行时, UAC 过滤令牌会得到 ERROR_ACCESS_DENIED, 整个
// 机制会退化成「全部当远程」, 回环共享永远拿不到本地路径的判定。
//
// 回环判定按对象身份 (卷序列号 + file id) 而不按主机名: localhost、127.0.0.1、
// 机器名、DNS 别名、hosts 文件条目都能指回本机, 名字比对既列不全也可被伪造。
// 文件系统自己报告的身份则与拼写无关: 打开共享根取 FILE_ID_INFO, 与本机各卷的
// 序列号比对; 命中后在该卷上按 file id 反查出本地路径, 再复核两者等价。后两步
// 任一失败即 LoopbackUnresolved：本机卷序列号匹配但未能确认本地路径，一律拒绝，
// 不得退化为 Remote。共享根打不开或身份查询失败时按 Remote 处理；这是分类的
// 回退规则，不能证明目标确实位于另一台机器。
// 已知限制: 本机卷表只枚举带盘符的卷, 落在无盘符挂载点卷上的共享根会因序列号不在
// 表中而判为 Remote。
//
// 身份来源与威胁模型: FILE_ID_INFO 由 SMB 服务端报告。恶意的、或与本机来自同一磁盘
// 镜像的服务端可以报出与本机某卷相同的身份, 使其共享被判为回环、按还原出的本地路径
// 规则判定。这不会让本机对象被读写 (实际 I/O 仍用调用方原始路径落在对方共享上), 也
// 不会比「真远程按非系统卷直通」拿到更多写权限; 但它确实让对方共享上的对象套用本机
// 路径的名单与库归属结论。
//
// ============================================

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
// WNetGetConnectionW 需要链接 Mpr.lib; 主工程与测试工程的 vcxproj 已登记, 新增包含方须自行链接。
#include <winnetwk.h>

namespace fb2k_utils {

enum class ShareKind { Remote, Loopback, LoopbackUnresolved };

struct ShareInfo {
    ShareKind kind = ShareKind::Remote;
    std::wstring localRoot;   // Loopback 时为本地根, 形如 X:\dir (无尾分隔符; 卷根本身为 X:\); 其余为空
};

// 对象身份: FILE_ID_INFO 取到时 volumeSerial 为 64 位、fileId 为 128 位;
// 回退 BY_HANDLE_FILE_INFORMATION 时 volumeSerial 为 32 位、fileId 高 64 位为 0。
// 比较使用记录的数值，不额外记录查询格式；不同格式的结果不保证相等或不等。
struct ObjectIdentity {
    uint64_t volumeSerial = 0;
    uint64_t fileIdLow = 0;
    uint64_t fileIdHigh = 0;
    bool operator==(const ObjectIdentity&) const = default;
};

struct LocalVolume {
    wchar_t letter = 0;       // 'C'..'Z'
    uint64_t volumeSerial = 0;
};

struct MappedDrive {
    wchar_t letter = 0;
    std::wstring shareRoot;   // \\host\share (无尾分隔符), 保留 WNetGetConnectionW 给出的原始拼写
};

// 全部系统调用的抽象。生产实现直接调 Win32 (Win32SystemProbe); 测试注入脚本化的假表。
class ISystemProbe {
public:
    virtual ~ISystemProbe() = default;
    // 打开 \\host\share\ 取身份。共享根打不开或两种身份查询都失败 -> nullopt。
    virtual std::optional<ObjectIdentity> QueryShareRootIdentity(std::wstring_view shareRoot) = 0;
    // FIXED / REMOVABLE / RAMDISK 盘符及其卷序列号 (排除 DRIVE_REMOTE, 那是映射盘)。
    virtual std::vector<LocalVolume> EnumerateLocalVolumes() = 0;
    // 在本机卷 letter 上按 fileId 打开对象并还原 DOS 路径; 失败 -> nullopt。
    virtual std::optional<std::wstring> ResolveLocalPathById(wchar_t letter, const ObjectIdentity& id) = 0;
    // fs::equivalent(uncRoot, localRoot) 复核; 异常按 false。
    virtual bool AreEquivalent(std::wstring_view uncRoot, std::wstring_view localRoot) = 0;
    // DRIVE_REMOTE 盘符 + WNetGetConnectionW 结果。
    virtual std::vector<MappedDrive> EnumerateMappedDrives() = 0;
    // 单调时钟, 毫秒。生产用 GetTickCount64; 测试可拨。
    virtual uint64_t NowMs() = 0;
};

namespace detail {

inline bool IsPathSeparator(wchar_t c) {
    return c == L'\\' || c == L'/';
}

}  // namespace detail

// ---- 纯字符串工具, 供 PathSecurity 与测试共用 ----
// header-only: 必须 inline, 否则 PathSecurity.h 带进多个编译单元后重复定义。

// \\host\share\rest -> (\\host\share, \rest)。正斜杠先统一为反斜杠; \\host\share
// 与 \\host\share\ 的 rest 都是空串; 不足 host + share 两段 (\\host、\\) 或不以
// 双分隔符开头 (C:\x) -> nullopt。
inline std::optional<std::pair<std::wstring, std::wstring>> SplitUncShareRoot(std::wstring_view path) {
    std::wstring p(path);
    std::replace(p.begin(), p.end(), L'/', L'\\');
    if (p.size() < 2 || p[0] != L'\\' || p[1] != L'\\') {
        return std::nullopt;
    }

    const size_t hostBegin = 2;
    const size_t hostEnd = p.find(L'\\', hostBegin);
    if (hostEnd == std::wstring::npos || hostEnd == hostBegin) {
        return std::nullopt;
    }

    const size_t shareBegin = hostEnd + 1;
    size_t shareEnd = p.find(L'\\', shareBegin);
    if (shareEnd == std::wstring::npos) {
        shareEnd = p.size();
    }
    if (shareEnd == shareBegin) {
        return std::nullopt;
    }

    std::wstring root = p.substr(0, shareEnd);
    std::wstring rest = p.substr(shareEnd);
    if (rest == L"\\") {
        rest.clear();
    }
    return std::make_pair(std::move(root), std::move(rest));
}

// 小写 host + share, 分隔符统一为反斜杠, 去尾分隔符。只用于比键 (缓存键、映射盘表
// 查找), 不用于对外输出——对外一律保留调用方或 WNetGetConnectionW 的原始拼写。
inline std::wstring NormalizeShareRoot(std::wstring_view shareRoot) {
    std::wstring s(shareRoot);
    for (wchar_t& c : s) {
        if (c == L'/') {
            c = L'\\';
        } else {
            c = static_cast<wchar_t>(std::towlower(c));
        }
    }
    while (!s.empty() && s.back() == L'\\') {
        s.pop_back();
    }
    return s;
}

// localRoot + rest 拼接, 去掉重复的分隔符: E:\ + \OST\x -> E:\OST\x; E:\OST + "" -> E:\OST;
// E:\OST + x -> E:\OST\x。
inline std::wstring JoinLocalRoot(std::wstring_view localRoot, std::wstring_view rest) {
    std::wstring joined(localRoot);
    const bool rootEndsWithSep = !joined.empty() && detail::IsPathSeparator(joined.back());
    const bool restStartsWithSep = !rest.empty() && detail::IsPathSeparator(rest.front());

    if (rootEndsWithSep && restStartsWithSep) {
        rest.remove_prefix(1);
    } else if (!rootEndsWithSep && !rest.empty() && !restStartsWithSep) {
        joined.push_back(L'\\');
    }
    joined.append(rest);
    return joined;
}

// 三张表 (共享分类、本机卷序列号、映射盘) 各带时间戳, TTL 60 秒, 过期懒刷新。
// 过期条目不删除: 下次访问同键时重新探测并覆盖, KnownLoopbackSharesCovering 枚举
// 全部条目 (含过期未重验的)。失败分类与成功分类同样缓存: 不缓存失败的话, 离线
// NAS 会让每次路径校验都付一次网络超时。
//
// 缓存键: 探针取到对象身份时用身份序列化的字符串, 同一共享的 \\localhost\E$ 与
// \\127.0.0.1\E$ 因此收敛到同一条目; 取不到身份时退回 NormalizeShareRoot 结果。
// 每个条目记着到达它的全部原拼写, 拼写展开时逐个返回。
//
// 映射盘表刷新后会对表中每个目标共享分类一次, 这样只经映射盘认识的回环共享也能
// 出现在本地路径的拼写展开里。代价: 映射表过期后的第一次校验会为每个映射盘付一次
// 共享根探测 (离线 NAS 是一次网络超时, 之后 60 秒内命中失败缓存)。这笔代价不是
// 一次性的: 只要某个映射目标离线, 每 60 秒重复一轮。触发方也不限于 UNC 路径——
// KnownLoopbackSharesCovering 无条件先刷映射表, 所以校验 E:\x.mp3 这样的本地路径
// 也可能等待无关离线 NAS 的网络超时。当前刷新会遍历所有映射目标，没有后台刷新
// 或跳过离线目标的机制。映射盘表保留 WNetGetConnectionW 的原始拼写，只在比键时归一化。
//
// 「从未加载」与「过期」分开表示: 时间戳用 std::optional, 空即从未加载。只靠时间戳
// 为 0 判断的话, 开机 60 秒内 GetTickCount64 的返回值小于 TTL, 空表会被当成新鲜表
// 服务。
//
// 单 std::mutex 守全部三张表; 探针调用在锁外进行——探针会打网络, 持锁打网络会把
// 其他线程上的校验一并卡住。做法是锁内查表决定要不要刷新, 解锁探测, 再加锁写回。
// std::mutex 不可重入, 映射盘表刷新后对各目标共享的分类循环因此也在锁外做。
class NetworkShareResolver {
public:
    static constexpr uint64_t kTtlMs = 60'000;

    explicit NetworkShareResolver(std::unique_ptr<ISystemProbe> probe)
        : probe_(std::move(probe)) {}

    // 分类 \\host\share。探针只拿调用方原拼写 (去尾分隔符再补一个) 去问; 归一化形态
    // 只用于比键, 因为回环共享的本地路径要靠这个原拼写反查, 假探针也按它脚本化。
    ShareInfo Classify(std::wstring_view shareRoot) {
        const std::wstring spelling = StripTrailingSeparators(shareRoot);
        const std::wstring normalized = NormalizeShareRoot(spelling);
        const uint64_t now = probe_->NowMs();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (const ShareEntry* hit = FindFreshBySpelling(normalized, now)) {
                return hit->info;
            }
        }

        const std::wstring rootWithSep = spelling + L"\\";
        const std::optional<ObjectIdentity> identity = probe_->QueryShareRootIdentity(rootWithSep);
        const std::wstring key = identity ? IdentityKey(*identity) : normalized;

        {
            // 另一拼写刚探测过同一对象: 只登记新拼写, 不再重复走卷表与 file id 反查。
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = shares_.find(key);
            if (it != shares_.end() && IsFresh(it->second.stampMs, now)) {
                BindSpelling(it->second, key, spelling, normalized);
                return it->second.info;
            }
        }

        ShareInfo info;
        if (identity) {
            const std::vector<LocalVolume> volumes = LocalVolumes(now);
            const auto vol = std::find_if(volumes.begin(), volumes.end(), [&](const LocalVolume& v) {
                return v.volumeSerial == identity->volumeSerial;
            });
            if (vol != volumes.end()) {
                // 已匹配本机卷序列号：路径还原或等价性复核失败时拒绝，不退化为 Remote。
                const std::optional<std::wstring> localRoot = probe_->ResolveLocalPathById(vol->letter, *identity);
                if (localRoot && probe_->AreEquivalent(rootWithSep, *localRoot)) {
                    info.kind = ShareKind::Loopback;
                    info.localRoot = *localRoot;
                } else {
                    info.kind = ShareKind::LoopbackUnresolved;
                }
            }
        }

        // 时间戳在探测之后重取: 一次离线探测可达数十秒, 若沿用探测前的 now, 条目会出生即
        // 接近过期, 失败缓存形同虚设。
        const uint64_t stamp = probe_->NowMs();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ShareEntry& entry = shares_[key];
            entry.info = info;
            entry.stampMs = stamp;
            BindSpelling(entry, key, spelling, normalized);
        }
        return info;
    }

    // 盘符不区分大小写。这里不调 GetDriveTypeW: 本地路径每次校验都会经过这里, 多一次
    // 系统调用就违背了「本地路径不变」; 盘符类型是 EnumerateMappedDrives 刷新表时的事。
    std::optional<std::wstring> ShareRootOfLetter(wchar_t letter) {
        const wchar_t wanted = static_cast<wchar_t>(std::towupper(letter));
        const std::vector<MappedDrive> mapped = MappedDrives(probe_->NowMs());
        for (const MappedDrive& drive : mapped) {
            if (static_cast<wchar_t>(std::towupper(drive.letter)) == wanted) {
                return drive.shareRoot;
            }
        }
        return std::nullopt;
    }

    // 映射到该共享的全部盘符 (按归一化拼写比对)。该共享已分类为 Loopback 时, 映射到
    // 同一对象其他拼写的盘符也算——回环共享的身份键已把不同拼写统一成一个对象。
    std::vector<wchar_t> LettersFor(std::wstring_view shareRoot) {
        const std::wstring normalized = NormalizeShareRoot(shareRoot);
        const std::vector<MappedDrive> mapped = MappedDrives(probe_->NowMs());

        std::vector<std::wstring> aliases{normalized};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto keyIt = spellingToKey_.find(normalized);
            if (keyIt != spellingToKey_.end()) {
                const auto entryIt = shares_.find(keyIt->second);
                if (entryIt != shares_.end() && entryIt->second.info.kind == ShareKind::Loopback) {
                    for (const std::wstring& s : entryIt->second.spellings) {
                        aliases.push_back(NormalizeShareRoot(s));
                    }
                }
            }
        }

        std::vector<wchar_t> letters;
        for (const MappedDrive& drive : mapped) {
            const std::wstring driveNormalized = NormalizeShareRoot(drive.shareRoot);
            if (std::find(aliases.begin(), aliases.end(), driveNormalized) != aliases.end()) {
                letters.push_back(drive.letter);
            }
        }
        return letters;
    }

    // 本地根覆盖 localPath 的全部已知回环共享, 每个原拼写各一条 (shareRootAsSeen, localRoot)。
    // 先刷新映射盘表: 只经映射盘认识的回环共享要靠表刷新时的分类才进入分类表。
    std::vector<std::pair<std::wstring, std::wstring>> KnownLoopbackSharesCovering(std::wstring_view localPath) {
        MappedDrives(probe_->NowMs());

        const std::wstring localPathStr(localPath);
        std::vector<std::pair<std::wstring, std::wstring>> out;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& kv : shares_) {
            const ShareEntry& entry = kv.second;
            if (entry.info.kind != ShareKind::Loopback) {
                continue;
            }
            if (!IsPathPrefixOf(entry.info.localRoot, localPathStr)) {
                continue;
            }
            for (const std::wstring& spelling : entry.spellings) {
                out.emplace_back(spelling, entry.info.localRoot);
            }
        }
        return out;
    }

private:
    struct ShareEntry {
        ShareInfo info;
        std::vector<std::wstring> spellings;      // 调用方原拼写 (去尾分隔符); 去重时按 NormalizeShareRoot 比
        std::optional<uint64_t> stampMs;          // 空 = 从未探测
    };

    // now <= stamp 也算新鲜: 另一线程可能已用更晚的时间戳写回, 本线程手里较早的 now
    // 不能因无符号下溢被误判为过期。
    static bool IsFresh(const std::optional<uint64_t>& stampMs, uint64_t now) {
        return stampMs.has_value() && (now <= *stampMs || now - *stampMs < kTtlMs);
    }

    static std::wstring StripTrailingSeparators(std::wstring_view s) {
        std::wstring out(s);
        while (out.size() > 2 && detail::IsPathSeparator(out.back())) {
            out.pop_back();
        }
        return out;
    }

    // 身份键以路径里不可能出现的 '|' 开头, 与 NormalizeShareRoot 产生的名字键不会撞。
    static std::wstring IdentityKey(const ObjectIdentity& id) {
        return L"|id|" + std::to_wstring(id.volumeSerial) + L'|' +
               std::to_wstring(id.fileIdHigh) + L'|' + std::to_wstring(id.fileIdLow);
    }

    // 与 PathSecurity 的同名判定语义一致: 大小写不敏感、分隔符感知、前缀自带尾分隔符即
    // 匹配。在本头重实现一份, 因为那一份是私有静态成员, 且本头不能依赖 PathSecurity.h。
    static bool IsPathPrefixOf(const std::wstring& prefix, const std::wstring& path) {
        if (path.size() < prefix.size()) return false;
        if (::_wcsnicmp(path.c_str(), prefix.c_str(), prefix.size()) != 0) return false;
        if (path.size() == prefix.size()) return true;
        if (detail::IsPathSeparator(prefix.back())) return true;
        return detail::IsPathSeparator(path[prefix.size()]);
    }

    // 锁内调用。
    const ShareEntry* FindFreshBySpelling(const std::wstring& normalized, uint64_t now) const {
        const auto keyIt = spellingToKey_.find(normalized);
        if (keyIt == spellingToKey_.end()) return nullptr;
        const auto entryIt = shares_.find(keyIt->second);
        if (entryIt == shares_.end() || !IsFresh(entryIt->second.stampMs, now)) return nullptr;
        return &entryIt->second;
    }

    // 锁内调用。把一个拼写登记到 key 对应的条目。该拼写此前若指向别的键 (对象身份变了,
    // 例如共享被删后只剩名字键), 从旧条目摘掉; 旧条目再无拼写可达时整条移除——否则它
    // 会带着过期的回环结论永远留在表里, 却再也没有拼写能触发对它的重验。
    void BindSpelling(ShareEntry& entry, const std::wstring& key,
                      const std::wstring& spelling, const std::wstring& normalized) {
        const auto sameSpelling = [&](const std::wstring& s) { return NormalizeShareRoot(s) == normalized; };

        const auto prev = spellingToKey_.find(normalized);
        if (prev != spellingToKey_.end() && prev->second != key) {
            const auto old = shares_.find(prev->second);
            if (old != shares_.end()) {
                std::vector<std::wstring>& oldSpellings = old->second.spellings;
                oldSpellings.erase(std::remove_if(oldSpellings.begin(), oldSpellings.end(), sameSpelling),
                                   oldSpellings.end());
                if (oldSpellings.empty()) {
                    shares_.erase(old);
                }
            }
        }
        spellingToKey_[normalized] = key;

        if (!std::any_of(entry.spellings.begin(), entry.spellings.end(), sameSpelling)) {
            entry.spellings.push_back(spelling);
        }
    }

    // 本机卷表, 按 TTL 缓存。探针在锁外; 时间戳在枚举之后重取。
    std::vector<LocalVolume> LocalVolumes(uint64_t now) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (IsFresh(volumesStampMs_, now)) {
                return volumes_;
            }
        }
        std::vector<LocalVolume> fresh = probe_->EnumerateLocalVolumes();
        const uint64_t stamp = probe_->NowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        volumes_ = fresh;
        volumesStampMs_ = stamp;
        return fresh;
    }

    // 映射盘表, 按 TTL 缓存; 新鲜则直接返回副本。刷新时先在锁外对每个目标共享分类, 再
    // 加锁写表: 表一旦可见, 其目标已在分类表里, KnownLoopbackSharesCovering 不会看到
    // 「有表无分类」的中间态。Classify 自己要拿 mutex_, 分类循环绝不能在持锁时做。
    // 代价: 分类循环期间表尚未写入, 此时并发进来的其他线程会各自再做一遍枚举与分类
    // (重复工作, 结果一致, 方向安全)。时间戳在分类循环结束后重取, 表的有效期从写表时刻
    // 起算——离线目标的探测可能耗时数十秒, 沿用循环前的 now 会让表出生即接近过期。
    // 这轮刷新 (含对每个目标的探测) 在映射表每次过期后都会重来一遍, 且由任何入口触发,
    // 包括只校验本地路径的 KnownLoopbackSharesCovering; 本单元不跳过任何目标。
    std::vector<MappedDrive> MappedDrives(uint64_t now) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (IsFresh(mappedStampMs_, now)) {
                return mapped_;
            }
        }
        std::vector<MappedDrive> fresh = probe_->EnumerateMappedDrives();
        for (const MappedDrive& drive : fresh) {
            Classify(drive.shareRoot);
        }
        const uint64_t stamp = probe_->NowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        mapped_ = fresh;
        mappedStampMs_ = stamp;
        return fresh;
    }

    std::unique_ptr<ISystemProbe> probe_;

    // 键: 有身份时用 IdentityKey, 没有时用 NormalizeShareRoot 结果
    std::unordered_map<std::wstring, ShareEntry> shares_;
    std::unordered_map<std::wstring, std::wstring> spellingToKey_;   // 归一化拼写 -> shares_ 键

    std::vector<LocalVolume> volumes_;
    std::optional<uint64_t> volumesStampMs_;   // 空 = 从未加载

    std::vector<MappedDrive> mapped_;
    std::optional<uint64_t> mappedStampMs_;    // 空 = 从未加载

    mutable std::mutex mutex_;
};

// ISystemProbe 的生产实现, 直接调 Win32。句柄纪律: 每个 CreateFileW / OpenFileById 成功后
// 的每条返回路径都 CloseHandle。128 位 file id 的字节序: FILE_ID_INFO 里的 Identifier 是
// 16 字节数组, 取回与写回 FILE_ID_DESCRIPTOR 用同样的 memcpy 顺序, 不做任何字节翻转;
// fileIdHigh == 0 作为「回退到 64 位 id 打开」的判据。
class Win32SystemProbe final : public ISystemProbe {
public:
    std::optional<ObjectIdentity> QueryShareRootIdentity(std::wstring_view shareRoot) override {
        const std::wstring root(shareRoot);
        HANDLE h = ::CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (h == INVALID_HANDLE_VALUE) return std::nullopt;
        const auto id = IdentityOf(h);
        ::CloseHandle(h);
        return id;
    }
    std::vector<LocalVolume> EnumerateLocalVolumes() override {
        std::vector<LocalVolume> out;
        const DWORD mask = ::GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if ((mask & (1u << i)) == 0) continue;
            const wchar_t letter = static_cast<wchar_t>(L'A' + i);
            const wchar_t root[] = {letter, L':', L'\\', 0};
            const UINT type = ::GetDriveTypeW(root);
            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE && type != DRIVE_RAMDISK) continue;
            HANDLE h = ::CreateFileW(root, FILE_READ_ATTRIBUTES,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (h == INVALID_HANDLE_VALUE) continue;
            if (const auto id = IdentityOf(h)) out.push_back({letter, id->volumeSerial});
            ::CloseHandle(h);
        }
        return out;
    }
    std::optional<std::wstring> ResolveLocalPathById(wchar_t letter, const ObjectIdentity& id) override {
        const wchar_t root[] = {letter, L':', L'\\', 0};
        HANDLE hVol = ::CreateFileW(root, FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (hVol == INVALID_HANDLE_VALUE) return std::nullopt;

        FILE_ID_DESCRIPTOR desc{};
        desc.dwSize = sizeof(desc);
        if (id.fileIdHigh != 0) {
            desc.Type = ExtendedFileIdType;
            std::memcpy(desc.ExtendedFileId.Identifier, &id.fileIdLow, 8);
            std::memcpy(desc.ExtendedFileId.Identifier + 8, &id.fileIdHigh, 8);
        } else {
            desc.Type = FileIdType;
            desc.FileId.QuadPart = static_cast<LONGLONG>(id.fileIdLow);
        }
        HANDLE hObj = ::OpenFileById(hVol, &desc, FILE_READ_ATTRIBUTES,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, FILE_FLAG_BACKUP_SEMANTICS);
        ::CloseHandle(hVol);
        if (hObj == INVALID_HANDLE_VALUE) return std::nullopt;

        std::optional<std::wstring> result;
        wchar_t stackBuf[MAX_PATH];
        DWORD len = ::GetFinalPathNameByHandleW(hObj, stackBuf, MAX_PATH, VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
        if (len > 0 && len < MAX_PATH) {
            result = StripExtendedPrefix(std::wstring(stackBuf, len));
        } else if (len >= MAX_PATH) {
            std::wstring big(len, L'\0');
            DWORD written = ::GetFinalPathNameByHandleW(hObj, big.data(), len, VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
            if (written > 0 && written < len) { big.resize(written); result = StripExtendedPrefix(big); }
        }
        ::CloseHandle(hObj);
        return result;
    }
    bool AreEquivalent(std::wstring_view uncRoot, std::wstring_view localRoot) override {
        try {
            return std::filesystem::equivalent(std::filesystem::path(uncRoot), std::filesystem::path(localRoot));
        } catch (...) { return false; }
    }
    std::vector<MappedDrive> EnumerateMappedDrives() override {
        std::vector<MappedDrive> out;
        const DWORD mask = ::GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if ((mask & (1u << i)) == 0) continue;
            const wchar_t letter = static_cast<wchar_t>(L'A' + i);
            const wchar_t root[] = {letter, L':', L'\\', 0};
            if (::GetDriveTypeW(root) != DRIVE_REMOTE) continue;
            const wchar_t local[] = {letter, L':', 0};
            wchar_t remote[1024];
            DWORD n = static_cast<DWORD>(std::size(remote));
            if (::WNetGetConnectionW(local, remote, &n) != NO_ERROR) continue;
            // 保留原始拼写 (只去尾分隔符): KnownLoopbackSharesCovering 要把它
            // 原样交给 is_path_addable, 用户注册库根用的大小写就是这个。
            std::wstring share(remote);
            while (share.size() > 2 && (share.back() == L'\\' || share.back() == L'/')) share.pop_back();
            out.push_back({letter, std::move(share)});
        }
        return out;
    }
    uint64_t NowMs() override { return ::GetTickCount64(); }

private:
    static std::optional<ObjectIdentity> IdentityOf(HANDLE h) {
        FILE_ID_INFO info{};
        if (::GetFileInformationByHandleEx(h, FileIdInfo, &info, sizeof(info))) {
            ObjectIdentity id;
            id.volumeSerial = info.VolumeSerialNumber;
            std::memcpy(&id.fileIdLow, info.FileId.Identifier, 8);
            std::memcpy(&id.fileIdHigh, info.FileId.Identifier + 8, 8);
            return id;
        }
        BY_HANDLE_FILE_INFORMATION basic{};
        if (::GetFileInformationByHandle(h, &basic)) {
            ObjectIdentity id;
            id.volumeSerial = basic.dwVolumeSerialNumber;
            id.fileIdLow = (static_cast<uint64_t>(basic.nFileIndexHigh) << 32) | basic.nFileIndexLow;
            id.fileIdHigh = 0;
            return id;
        }
        return std::nullopt;
    }
    // \\?\E:\dir -> E:\dir ; 去尾分隔符 (根目录 E:\ 保留)
    static std::wstring StripExtendedPrefix(std::wstring s) {
        if (s.starts_with(L"\\\\?\\UNC\\")) s = L"\\\\" + s.substr(8);
        else if (s.starts_with(L"\\\\?\\")) s = s.substr(4);
        if (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
        return s;
    }
};

inline std::unique_ptr<ISystemProbe> MakeWin32SystemProbe() {
    return std::make_unique<Win32SystemProbe>();
}

}  // namespace fb2k_utils
