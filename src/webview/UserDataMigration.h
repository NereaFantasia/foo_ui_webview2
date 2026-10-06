#pragma once
// ============================================
// UserDataMigration.h - 把旧的共用用户数据目录复制成便携实例自己的目录
// ============================================
//
// 步骤照 WebView2 文档「Manage user data folders」的搬迁做法：先确认没有 WebView2
// 会话在用旧目录，再把内容搬到新位置，最后用新位置启动。两处与文档不同：
// - 复制而不是移动。旧目录仍是安装版的用户数据目录，还没升级的实例也在用它。
// - 运行时会重建的缓存与崩溃转储不复制（IsRebuildable），否则首次启动要多拷几百 MB。
//
// 只依赖标准库与 Win32，tests 工程可以直接包含。
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace webview_udf {

enum class CopyOutcome {
    // 源目录不存在，或目标目录已存在（已经迁移过，或实例已经在用它）
    NotNeeded,
    Copied,
    Failed,
};

// 迁移时跳过的条目：浏览器的锁文件、崩溃转储与各类缓存。relative 相对用户数据目录，
// 比较不分大小写；命中的是目录时整个目录跳过。名单取自 WebView2 运行时实际创建的目录。
inline bool IsRebuildable(const std::filesystem::path& relative) {
    static constexpr std::array<std::wstring_view, 12> kSkipped{{
        L"EBWebView\\lockfile",
        L"EBWebView\\Crashpad",
        L"EBWebView\\GrShaderCache",
        L"EBWebView\\ShaderCache",
        L"EBWebView\\GPUPersistentCache",
        L"EBWebView\\component_crx_cache",
        L"EBWebView\\extensions_crx_cache",
        L"EBWebView\\Default\\Cache",
        L"EBWebView\\Default\\Code Cache",
        L"EBWebView\\Default\\GPUCache",
        L"EBWebView\\Default\\DawnGraphiteCache",
        L"EBWebView\\Default\\DawnWebGPUCache",
    }};
    const std::wstring text = relative.lexically_normal().make_preferred().wstring();
    const auto sameIgnoringCase = [&text](std::wstring_view candidate) {
        return text.size() == candidate.size() &&
               std::equal(text.begin(), text.end(), candidate.begin(), [](wchar_t a, wchar_t b) {
                   return std::towlower(a) == std::towlower(b);
               });
    };
    return std::ranges::any_of(kSkipped, sameIgnoringCase);
}

// 浏览器进程运行期间独占打开 EBWebView\lockfile，退出时删掉它。所以文件不在，或能
// 独占打开，就说明没有会话在用这个目录。
inline bool IsInUse(const std::filesystem::path& userDataFolder) {
    const std::filesystem::path lockfile = userDataFolder / L"EBWebView" / L"lockfile";
    HANDLE handle = CreateFileW(lockfile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return false;
    }
    return GetLastError() == ERROR_SHARING_VIOLATION;
}

// 宿主刚退出时它的浏览器进程可能还没退完（foobar2000 装完组件自动重启就是这样），
// 所以占用时先等一会儿。返回等到最后是否仍被占用。
inline bool WaitWhileInUse(const std::filesystem::path& userDataFolder,
                           std::chrono::milliseconds timeout,
                           std::chrono::milliseconds poll = std::chrono::milliseconds(100)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (IsInUse(userDataFolder)) {
        if (std::chrono::steady_clock::now() >= deadline) return true;
        std::this_thread::sleep_for(poll);
    }
    return false;
}

namespace detail {

// 把迭代器所指的条目复制到 staging 下的同一相对位置；可重建的目录整个跳过，迭代器
// 不再进入它。出错时写 ec。
inline void CopyEntry(std::filesystem::recursive_directory_iterator& it,
                      const std::filesystem::path& source,
                      const std::filesystem::path& staging,
                      std::error_code& ec) {
    namespace fs = std::filesystem;
    if (const fs::path relative = it->path().lexically_relative(source); IsRebuildable(relative)) {
        if (it->is_directory(ec)) it.disable_recursion_pending();
    } else if (it->is_directory(ec)) {
        fs::create_directories(staging / relative, ec);
    } else if (it->is_regular_file(ec)) {
        fs::copy_file(it->path(), staging / relative, fs::copy_options::overwrite_existing, ec);
    }
    // 其余类型（链接、设备）跳过：Chromium 不在用户数据目录里放这些
}

}  // namespace detail

// 把 source 的内容复制成 target。先复制到同级的 "<target>.migrating"，全部成功后再改名，
// 所以中途失败或进程被杀都不会留下半个 target；上次留下的临时目录先删掉。失败时 error
// 写明原因。调用方负责先确认 source 没被占用。
inline CopyOutcome CopyUserDataFolder(const std::filesystem::path& source,
                                      const std::filesystem::path& target,
                                      std::wstring* error = nullptr) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(target, ec) || !fs::is_directory(source, ec)) return CopyOutcome::NotNeeded;

    const auto fail = [error](std::wstring_view what, const std::error_code& code) {
        if (error) {
            *error = what;
            if (code) *error += L": " + fs::path(code.message()).wstring();
        }
        return CopyOutcome::Failed;
    };

    fs::path staging = target;
    staging += L".migrating";
    fs::remove_all(staging, ec);
    if (ec) return fail(L"cannot clear " + staging.wstring(), ec);
    if (!fs::create_directories(staging, ec)) return fail(L"cannot create " + staging.wstring(), ec);

    const auto discardStaging = [&staging]() {
        std::error_code ignored;
        fs::remove_all(staging, ignored);
    };

    fs::recursive_directory_iterator it(source, fs::directory_options::none, ec);
    if (ec) {
        discardStaging();
        return fail(L"cannot read " + source.wstring(), ec);
    }
    const fs::recursive_directory_iterator end;
    while (!ec && it != end) {
        detail::CopyEntry(it, source, staging, ec);
        if (ec) {
            discardStaging();
            return fail(L"cannot copy " + it->path().wstring(), ec);
        }
        it.increment(ec);
    }
    if (ec) {
        discardStaging();
        return fail(L"cannot walk " + source.wstring(), ec);
    }

    fs::rename(staging, target, ec);
    if (ec) {
        discardStaging();
        return fail(L"cannot rename " + staging.wstring(), ec);
    }
    return CopyOutcome::Copied;
}

}  // namespace webview_udf
