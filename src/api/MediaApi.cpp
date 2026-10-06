#include "pch.h"
#include "api/MediaApi.h"
#include "api/TypedApi.h"
#include "api/generated/MediaSchema.h"
#include "core/WebViewContext.h"
#include "webview/WebViewHost.h"
#include "media/MediaFile.h"
#include "media/MediaService.h"
#include "media/ChapterSubsongs.h"
#include "media/ContainerInfo.h"
#include "media/ContainerReader.h"
#include "utils/SubsongUtils.h"
#include <deque>

namespace {
void SendCompletionFailure(const DeferredResponder& responder) noexcept {
    try {
        responder.SendJson(api::results::FailureToJson(api::Fail("Unable to complete media request", ApiErrorCode::OPERATION_FAILED)));
    } catch (...) {
        // 错误响应自身也可能分配失败；不能让第二次异常越过主线程回调边界。
    }
}
struct InspectionFailure {
    std::string error;
    std::string code;
};
// 后台结果只携带自有数据；错误信封在主线程构造，避免把 JSON 对象带入跨线程闭包。
struct Inspection {
    media::FileIdentity identity;
    std::wstring finalPath;
    std::string mimeType;
    std::optional<api::media::GetContainerInfoResult> container;
    std::optional<InspectionFailure> failure;
};
struct CacheEntry {
    std::wstring path;
    media::FileIdentity identity;
    api::media::GetContainerInfoResult info;
    std::size_t bytes = 0;
};
// 在媒体 IO 线程打开 foobar2000 的输入，只读子曲目标识与时长；任何失败都按「没有对应」处理。
std::vector<media::SubsongLength> ReadSubsongs(const std::wstring& path) {
    std::vector<media::SubsongLength> subsongs;
    try {
        pfc::string8 canonical;
        filesystem::g_get_canonical_path(media::Utf8(path).c_str(), canonical);
        abort_callback_impl abort;
        input_info_reader::ptr reader;
        input_entry::g_open_for_info_read(reader, nullptr, canonical, abort);
        if (!reader.is_valid()) return {};
        const auto count = reader->get_subsong_count();
        if (count < 2 || count > media::detail::Reader::kEntryLimit) return {};
        for (t_uint32 index = 0; index < count; ++index) {
            if (media::detail::ioStopping.load(std::memory_order_acquire)) return {};
            const auto subsong = reader->get_subsong(index);
            file_info_impl info;
            reader->get_info(subsong, info, abort);
            const double length = info.get_length();
            subsongs.emplace_back(static_cast<std::int64_t>(subsong), length > 0 ? std::optional<double>(length) : std::nullopt);
        }
    } catch (...) {
        return {};
    }
    return subsongs;
}
api::media::GetContainerInfoResult InspectCached(media::MediaFile& file, const std::wstring& path,
                                                media::FileIdentity identity) {
    static std::mutex mutex;
    static std::deque<CacheEntry> entries;
    static std::size_t bytes = 0;
    // 缓存命中只省去解析；调用方仍须复查文件身份、当前文档和路径权限。
    {
        std::lock_guard lock(mutex);
        for (const auto& entry : entries) if (entry.path == path && entry.identity == identity) return entry.info;
    }
    auto result = media::InspectContainer(identity.size, [&file](std::uint64_t offset, std::size_t length) { return file.Read(offset, length); });
    if (file.Identity() != identity) throw media::FileError("file-changed", ERROR_FILE_INVALID);
    if (result.chapters && result.chapters->size() > 1) media::AssignChapterSubsongs(*result.chapters, ReadSubsongs(path));
    // 用序列化长度限制缓存规模；这是容量估算，不是容器对象的实际堆占用。
    const auto resultBytes = api::media::ToJson(result).dump().size();
    if (resultBytes > 256u * 1024u) return result;
    {
        std::lock_guard lock(mutex);
        std::erase_if(entries, [&path](const CacheEntry& entry) {
            if (entry.path != path) return false;
            bytes -= entry.bytes;
            return true;
        });
        while (!entries.empty() && (entries.size() >= 32 || bytes + resultBytes > 4u * 1024u * 1024u)) {
            bytes -= entries.front().bytes;
            entries.pop_front();
        }
        entries.emplace_back(path, identity, result, resultBytes);
        bytes += resultBytes;
    }
    return result;
}
Inspection InspectFile(const std::wstring& path, bool container) {
    Inspection result;
    try {
        media::MediaFile file(path);
        result.identity = file.Identity();
        result.finalPath = file.FinalPath();
        result.mimeType = file.MimeType();
        if (container) result.container = InspectCached(file, result.finalPath, result.identity);
        if (file.Identity() != result.identity) throw media::FileError("file-changed", ERROR_FILE_INVALID);
    } catch (const media::FileError& error) {
        auto code = ApiErrorCode::OPERATION_FAILED;
        if (error.code == ERROR_FILE_NOT_FOUND || error.code == ERROR_PATH_NOT_FOUND) code = ApiErrorCode::NOT_FOUND;
        else if (error.code == ERROR_ACCESS_DENIED) code = ApiErrorCode::PERMISSION_DENIED;
        result.failure = InspectionFailure{error.what(), code};
    } catch (const std::exception& error) {
        result.failure = InspectionFailure{error.what(), ApiErrorCode::OPERATION_FAILED};
    }
    return result;
}
void CompleteInspection(HWND hwnd, const WebViewHost::DocumentStamp& stamp, const std::wstring& native,
                        const std::string& origin, const DeferredResponder& responder, bool container, const Inspection& inspected) {
    auto* current = WebViewContext::GetInstance().GetWebViewHost(hwnd);
    if (!current || !current->IsCurrentDocument(stamp)) return;
    // 路径可能经过重解析点；原路径与打开句柄得到的最终路径都要通过权限检查。
    if (current->GetMediaOrigin() != origin || !media::CanRead(native) ||
        (!inspected.finalPath.empty() && !media::CanRead(inspected.finalPath))) {
        responder.SendJson(api::results::FailureToJson(api::Fail("Media access is no longer allowed", ApiErrorCode::PERMISSION_DENIED)));
        return;
    }
    if (inspected.failure) {
        responder.SendJson(api::results::FailureToJson(api::Fail(inspected.failure->error, inspected.failure->code)));
        return;
    }
    if (container) {
        api::Send(responder, api::Result<api::media::GetContainerInfoResult>(*inspected.container));
        return;
    }
    const auto token = media::Tokens().Mint({stamp.hostSerial, stamp.navigationGeneration, stamp.hostGeneration},
        origin, inspected.finalPath, inspected.identity, inspected.mimeType);
    if (!token) {
        responder.SendJson(api::results::FailureToJson(api::Fail("Unable to issue a media token", ApiErrorCode::OPERATION_FAILED)));
        return;
    }
    api::media::GetStreamUrlResult result;
    result.url = media::Utf8(media::kUrlPrefix) + *token;
    result.size = static_cast<std::int64_t>(inspected.identity.size);
    result.mimeType = inspected.mimeType;
    api::Send(responder, api::Result<api::media::GetStreamUrlResult>(std::move(result)));
}
void Start(const std::string& path, const CallerContext& caller, const DeferredResponder& responder, bool container) {
    auto* host = WebViewContext::GetInstance().GetWebViewHost(caller.callerHwnd);
    const auto origin = host ? host->GetMediaOrigin() : std::string();
    if (!host || origin.empty()) {
        responder.SendJson(api::results::FailureToJson(api::Fail("Media access requires a trusted HTTP(S) document", ApiErrorCode::PERMISSION_DENIED)));
        return;
    }
    if (!container && !host->CanServeMedia()) {
        responder.SendJson(api::results::FailureToJson(api::Fail("Media routing requires WebView2 ICoreWebView2_22", ApiErrorCode::NOT_SUPPORTED)));
        return;
    }
    std::string resolved;
    if (!SubsongUtils::TryResolveNativeMediaPath(path, resolved)) {
        responder.SendJson(api::results::FailureToJson(api::Fail("A local container path is required", ApiErrorCode::INVALID_PARAMS)));
        return;
    }
    const auto native = media::Wide(SubsongUtils::ParseSubsongPath(resolved).first);
    if (!media::CanRead(native)) {
        responder.SendJson(api::results::FailureToJson(api::Fail("Media path is not allowed", ApiErrorCode::PERMISSION_DENIED)));
        return;
    }
    // HWND 可以复用，异步回包必须同时核对宿主实例、导航与 WebView 代次。
    // worker 不持有 host 指针，也不读取依赖宿主状态的权限和令牌表。
    const auto stamp = host->CaptureDocument();
    const auto hwnd = caller.callerHwnd;
    const bool queued = media::QueueMediaIo([native = native, origin = origin, stamp, hwnd, responder, container] {
        auto inspected = InspectFile(native, container);
        fb2k::inMainThread([native = native, origin = origin, stamp, hwnd, responder, container, inspected = std::move(inspected)] {
            try {
                CompleteInspection(hwnd, stamp, native, origin, responder, container, inspected);
            } catch (...) { SendCompletionFailure(responder); }
        });
    }, [responder] {
        SendCompletionFailure(responder);
    });
    if (!queued) responder.SendJson(api::results::FailureToJson(api::Fail("Media IO queue is busy or stopping", ApiErrorCode::OPERATION_FAILED)));
}
void GetStreamUrl(const api::media::GetStreamUrlParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    Start(params.path, caller, responder, false);
}
void GetContainerInfo(const api::media::GetContainerInfoParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    Start(params.path, caller, responder, true);
}
}
void RegisterMediaApi() {
    api::RegisterApiDeferred("media.getStreamUrl", GetStreamUrl);
    api::RegisterApiDeferred("media.getContainerInfo", GetContainerInfo);
}
