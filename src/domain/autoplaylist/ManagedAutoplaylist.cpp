#include "pch.h"
#include "ManagedAutoplaylist.h"
#include "utils/GuidUtils.h"
#include <foobar2000/SDK/autoplaylist.h>
#include <foobar2000/SDK/search_tools.h>
#include <limits>

namespace managed_autoplaylist {
namespace {

// 此身份用于持久化恢复，发布后必须保持稳定且不与其他提供者复用。
constexpr GUID ProviderGuid{0xd4f4ea20, 0xaf70, 0x4b20, {0xb9, 0xf6, 0x0a, 0x36, 0x7e, 0xac, 0x09, 0x17}};

ChangeCallback& ChangeSink() {
    static ChangeCallback callback;
    return callback;
}

void ReportCallbackFailure(const char* operation) noexcept {
    try { LOG("Managed autoplaylist callback failed: ", operation); } catch (...) {}
}

struct RefreshState {
    uint64_t generation = 0;
    bool alive = true;
    completion_notify::ptr managerNotify;
};

struct NativeCompiled final : CompiledConfig {
    Config config;
    search_filter_v2::ptr filter;
    titleformat_object::ptr sort;
    int direction = 1;
    uint64_t generation = 0;
};

class ManagedClient : public autoplaylist_client_v2 {
    FB2K_MAKE_SERVICE_INTERFACE(ManagedClient, autoplaylist_client_v2);
public:
    virtual ConfigState& State() = 0;
    virtual std::shared_ptr<const CompiledConfig> CompileCandidate(const Config& config) = 0;
    virtual void PublishCandidate(std::shared_ptr<const CompiledConfig> compiled) = 0;
    virtual void NotifyConfigurationChanged() = 0;
};

const GUID ManagedClient::class_guid{0x8e59746c, 0x22d4, 0x4939, {0xa9, 0x1a, 0x48, 0x38, 0x62, 0x1a, 0x24, 0x63}};

class NativeClient : public ManagedClient {
public:
    explicit NativeClient(Config config)
        : state_(std::move(config)), refresh_(std::make_shared<RefreshState>()) {
        core_api::ensure_main_thread();
        PublishCandidate(CompileCandidate(state_.Published()));
    }

    ~NativeClient() { refresh_->alive = false; }

    GUID get_guid() override { return ProviderGuid; }
    ConfigState& State() override { return state_; }

    void filter(metadb_handle_list_cref items, bool* output) override {
        core_api::ensure_main_thread();
        const auto snapshot = compiled_;
        snapshot->filter->test_multi_ex(items, output, fb2k::noAbort);
    }

    bool sort(metadb_handle_list_cref items, size_t* order) override {
        core_api::ensure_main_thread();
        const auto snapshot = compiled_;
        if (snapshot->sort.is_empty()) return false;
        metadb_handle_list_helper::sort_by_format_get_order(items, order, snapshot->sort, nullptr, snapshot->direction);
        return true;
    }

    void get_configuration(stream_writer* stream, abort_callback& abort) override {
        core_api::ensure_main_thread();
        const auto snapshot = compiled_;
        const auto bytes = EncodeConfiguration(snapshot->config);
        stream->write(bytes.data(), bytes.size(), abort);
    }

    void show_ui(size_t) override {}
    bool show_ui_available() override { return false; }
    void get_display_name(pfc::string_base& output) override { output = "WebView UI Autoplaylist"; }
    void set_full_refresh_notify(completion_notify::ptr notify) override {
        core_api::ensure_main_thread();
        if (notify.is_valid()) {
            // 同一 service 对象可以重新挂接；通知对象地址相同也不能沿用旧会话。
            if (hasAttached_) state_.BeginSession();
            hasAttached_ = true;
        }
        refresh_->managerNotify = std::move(notify);
    }

    std::shared_ptr<const CompiledConfig> CompileCandidate(const Config& config) override {
        core_api::ensure_main_thread();
        auto result = std::make_shared<NativeCompiled>();
        result->config = config;
        result->generation = ++nextGeneration_;
        const std::weak_ptr<RefreshState> weak = refresh_;
        const auto generation = result->generation;
        auto notification = fb2k::makeCompletionNotify([weak, generation](unsigned) noexcept {
            try {
                // 总是排队，避免在编译或原生通知栈里嵌套重建；废弃候选也有独立 generation。
                fb2k::inMainThread([weak, generation]() noexcept {
                    try {
                        core_api::ensure_main_thread();
                        const auto owner = weak.lock();
                        if (!owner || !owner->alive || owner->generation != generation) return;
                        const auto notify = owner->managerNotify;
                        if (notify.is_valid()) notify->on_completion(0);
                    } catch (...) { ReportCallbackFailure("time refresh"); }
                });
            } catch (...) { ReportCallbackFailure("queue time refresh"); }
        });
        try {
            result->filter = search_filter_manager_v2::get()->create_ex(config.query.c_str(), notification,
                search_filter_manager_v2::KFlagAllowSort);
        } catch (const std::exception& error) {
            throw ValidationError("query", error.what());
        }
        result->filter->get_sort_pattern(result->sort, result->direction);
        search_filter_v3::ptr extended;
        const bool explicitSort = result->filter->service_query_t(extended) && extended->is_sort_explicit();
        if (!config.sort.empty()) {
            if (explicitSort) throw ValidationError("sort", "Independent sort conflicts with query SORT BY");
            if (!titleformat_compiler::get()->compile(result->sort, config.sort.c_str())) {
                throw ValidationError("sort", "The host rejected the sort expression");
            }
            result->direction = 1;
        }
        return result;
    }

    void PublishCandidate(std::shared_ptr<const CompiledConfig> candidate) override {
        // 此私有接口只接收 CompileCandidate 返回的对象；发布时不调用宿主或分配内存。
        compiled_ = std::static_pointer_cast<const NativeCompiled>(std::move(candidate));
        refresh_->generation = compiled_->generation;
    }

    void NotifyConfigurationChanged() override {
        core_api::ensure_main_thread();
        const auto notify = refresh_->managerNotify;
        if (notify.is_empty()) throw std::runtime_error("Autoplaylist manager did not install a refresh callback");
        notify->on_completion(1);
    }

private:
    ConfigState state_;
    std::shared_ptr<RefreshState> refresh_;
    std::shared_ptr<const NativeCompiled> compiled_;
    uint64_t nextGeneration_ = 0;
    bool hasAttached_ = false;
};

std::vector<uint8_t> ReadStoredConfiguration(stream_reader* stream, size_t sizeHint, abort_callback& abort) {
    constexpr size_t maximum = 2 * MaxTextBytes + 16;
    if (sizeHint != SIZE_MAX && sizeHint > maximum) throw ValidationError("configuration", "Configuration exceeds size limit");
    stream_reader_limited_ref bounded(stream, sizeHint == SIZE_MAX ? maximum : sizeHint);
    std::vector<uint8_t> bytes(8);
    bounded.read_object(bytes.data(), bytes.size(), abort);
    for (unsigned field = 0; field < 2; ++field) {
        const auto size = bounded.read_lendian_t<uint32_t>(abort);
        if (size > MaxTextBytes) throw ValidationError("configuration", "Configuration text exceeds size limit");
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<uint8_t>(size >> shift));
        const auto offset = bytes.size();
        bytes.resize(offset + size);
        bounded.read_object(bytes.data() + offset, size, abort);
    }
    // sizeHint 不承诺流末尾，读完该格式立即停止，避免吞掉相邻配置。
    return bytes;
}

class NativeFactory : public autoplaylist_client_factory {
public:
    GUID get_guid() override { return ProviderGuid; }
    autoplaylist_client_ptr instantiate(stream_reader* stream, size_t sizeHint, abort_callback& abort) override {
        core_api::ensure_main_thread();
        return fb2k::service_new<NativeClient>(DecodeConfiguration(ReadStoredConfiguration(stream, sizeHint, abort)));
    }
};

static service_factory_single_t<NativeFactory> Factory;

std::optional<size_t> Locate(const GUID& guid) {
    const auto playlists = playlist_manager_v5::get();
    const auto index = playlists->find_playlist_by_guid(guid);
    if (index >= playlists->get_playlist_count()) return std::nullopt;
    if (playlists->playlist_get_guid(index) != guid) return std::nullopt;
    return index;
}

ManagedClient::ptr QueryManaged(size_t index) {
    const auto playlists = playlist_manager_v5::get();
    if (index >= playlists->get_playlist_count()) return {};
    const auto guid = playlists->playlist_get_guid(index);
    const auto manager = autoplaylist_manager::get();
    if (!manager->is_client_present(index)) return {};
    const auto client = manager->query_client(index);
    ManagedClient::ptr managed;
    if (client.is_empty() || client->get_guid() != ProviderGuid || !client->service_query_t(managed)) return {};
    if (Locate(guid) != std::optional<size_t>(index)) return {};
    return managed;
}

struct PinnedClient {
    GUID guid{};
    ManagedClient::ptr client;
};

PinnedClient Pin(const std::string& playlistGuid) {
    GUID guid{};
    if (!GuidUtils::StringToGuid(playlistGuid, guid)) throw ValidationError("playlistGuid", "Invalid playlist GUID");
    const auto index = Locate(guid);
    if (!index) throw std::runtime_error("Playlist no longer exists");
    auto client = QueryManaged(*index);
    if (client.is_empty()) throw std::runtime_error("Playlist provider is no longer managed");
    if (Locate(guid) != index) throw std::runtime_error("Playlist changed while resolving provider");
    return {guid, std::move(client)};
}

class NativeHost final : public IUpdateHost {
public:
    explicit NativeHost(PinnedClient pin) : pin_(std::move(pin)) {}

    bool IsCurrent() override {
        const auto index = Locate(pin_.guid);
        if (!index) return false;
        const auto actual = QueryManaged(*index);
        return actual == pin_.client && Locate(pin_.guid) == index;
    }

    bool ReadKeepSorted() override {
        EnsureCurrent();
        const auto flags = autoplaylist_manager_v2::get()->get_client_flags(autoplaylist_client::ptr(pin_.client));
        EnsureCurrent();
        return (flags & autoplaylist_flag_sort) != 0;
    }

    void SetKeepSorted(bool value) override {
        EnsureCurrent();
        const auto manager = autoplaylist_manager_v2::get();
        const auto flags = manager->get_client_flags(autoplaylist_client::ptr(pin_.client));
        EnsureCurrent();
        const auto updated = value ? flags | autoplaylist_flag_sort : flags & ~static_cast<uint32_t>(autoplaylist_flag_sort);
        manager->set_client_flags(autoplaylist_client::ptr(pin_.client), updated);
        EnsureCurrent();
    }

    std::shared_ptr<const CompiledConfig> Compile(const Config& candidate) override {
        EnsureCurrent();
        auto compiled = pin_.client->CompileCandidate(candidate);
        EnsureCurrent();
        return compiled;
    }

    void Publish(const Config&, std::shared_ptr<const CompiledConfig> compiled) override {
        EnsureCurrent();
        pin_.client->PublishCandidate(std::move(compiled));
    }

    void Notify() override {
        EnsureCurrent();
        pin_.client->NotifyConfigurationChanged();
        EnsureCurrent();
    }

    void Changed(const Snapshot& current) override {
        EnsureCurrent();
        const auto callback = ChangeSink();
        if (callback) callback(GuidUtils::GuidToString(pin_.guid), current.revision);
    }

private:
    void EnsureCurrent() {
        if (!IsCurrent()) throw std::runtime_error("Playlist or provider changed");
    }
    PinnedClient pin_;
};

Failure MakeFailure(const char* reason, const char* stage) {
    Failure failure;
    failure.reason = reason;
    failure.stage = stage;
    return failure;
}

std::string CleanupCreated(const GUID& guid, const ManagedClient::ptr& expected) {
    try {
        const auto playlists = playlist_manager_v5::get();
        const auto manager = autoplaylist_manager::get();
        auto index = Locate(guid);
        if (!index) return "removed";
        if (manager->is_client_present(*index)) {
            const auto actual = manager->query_client(*index);
            if (actual != autoplaylist_client::ptr(expected)) return Locate(guid) ? "retained" : "removed";
        } else if (playlists->playlist_lock_is_present(*index)) {
            return Locate(guid) ? "retained" : "removed";
        }
        if (Locate(guid) != index) return "unknown";
        try { playlists->remove_playlist(*index); } catch (...) {}
        return Locate(guid) ? "retained" : "removed";
    } catch (...) {
        return "unknown";
    }
}

} // namespace

bool IsManaged(size_t index) {
    core_api::ensure_main_thread();
    return QueryManaged(index).is_valid();
}

void SetChangeCallback(ChangeCallback callback) {
    core_api::ensure_main_thread();
    ChangeSink() = std::move(callback);
}

void ScheduleFlagsObservation(size_t index) {
    try {
        core_api::ensure_main_thread();
        const auto playlists = playlist_manager_v5::get();
        if (index >= playlists->get_playlist_count()) return;
        const auto guid = playlists->playlist_get_guid(index);
        auto client = QueryManaged(index);
        if (client.is_empty() || Locate(guid) != std::optional<size_t>(index)) return;
        PinnedClient pin{guid, client};
        fb2k::inMainThread([pin = std::move(pin)]() noexcept {
            try {
                core_api::ensure_main_thread();
                NativeHost host(pin);
                if (!host.IsCurrent()) return;
                (void)pin.client->State().Read(host);
            } catch (...) { ReportCallbackFailure("flags observation"); }
        });
    } catch (...) { ReportCallbackFailure("queue flags observation"); }
}

Operation Read(const std::string& playlistGuid) {
    core_api::ensure_main_thread();
    try {
        const auto pin = Pin(playlistGuid);
        NativeHost host(pin);
        return {playlistGuid, pin.client->State().Read(host), {}};
    } catch (...) {
        return {playlistGuid, MakeFailure("targetChanged", "resolve"), {}};
    }
}

Operation Update(const std::string& playlistGuid, const Patch& patch, const std::string& expectedRevision) {
    core_api::ensure_main_thread();
    try {
        const auto pin = Pin(playlistGuid);
        NativeHost host(pin);
        return {playlistGuid, pin.client->State().Update(patch, expectedRevision, host), {}};
    } catch (...) {
        return {playlistGuid, MakeFailure("targetChanged", "resolve"), {}};
    }
}

Operation Create(const std::string& name, Config config) {
    core_api::ensure_main_thread();
    ManagedClient::ptr client;
    std::optional<GUID> guid;
    std::string guidText;
    const char* stage = "validate";
    bool creationAttempted = false;
    try {
        ValidateText(name, "name");
        ValidateText(config.query, "query", true);
        ValidateText(config.sort, "sort");
        const uint32_t initialFlags = config.keepSorted ? autoplaylist_flag_sort : 0;
        stage = "compile";
        client = fb2k::service_new<NativeClient>(std::move(config));
        stage = "create";
        creationAttempted = true;
        const auto playlists = playlist_manager_v5::get();
        const auto index = playlists->create_playlist(name.c_str(), name.size(), SIZE_MAX);
        if (index >= playlists->get_playlist_count()) throw std::runtime_error("Playlist creation failed");
        guid = playlists->playlist_get_guid(index);
        guidText = GuidUtils::GuidToString(*guid);
        stage = "attach";
        auto current = Locate(*guid);
        if (!current) throw std::runtime_error("Created playlist disappeared");
        autoplaylist_manager::get()->add_client(client, *current, initialFlags);
        NativeHost host({*guid, client});
        if (!host.IsCurrent()) throw std::runtime_error("Created provider changed");
        const auto outcome = client->State().Read(host);
        if (std::holds_alternative<Failure>(outcome)) throw std::runtime_error("Created configuration could not be read back");
        return {guidText, outcome, {}};
    } catch (const ValidationError& error) {
        auto failure = MakeFailure("invalidConfiguration", stage);
        failure.invalidParams = true;
        failure.param = error.param;
        return {guidText, std::move(failure), guid ? CleanupCreated(*guid, client) : creationAttempted ? "unknown" : "notCreated"};
    } catch (...) {
        auto failure = MakeFailure("createFailed", stage);
        return {guidText, std::move(failure), guid ? CleanupCreated(*guid, client) : creationAttempted ? "unknown" : "notCreated"};
    }
}

} // namespace managed_autoplaylist
