#include "pch.h"
#include "domain/library/LibrarySync.h"

namespace library_sync {
namespace {

// {4784E652-4743-42E9-A738-A70D9998D18F}
constexpr GUID guid_cfg_epoch =
    { 0x4784e652, 0x4743, 0x42e9, { 0xa7, 0x38, 0xa7, 0x0d, 0x99, 0x98, 0xd1, 0x8f } };
// {EF87C572-61EE-4CD0-ABA3-3B663C767EAE}
constexpr GUID guid_cfg_revision =
    { 0xef87c572, 0x61ee, 0x4cd0, { 0xab, 0xa3, 0x3b, 0x66, 0x3c, 0x76, 0x7e, 0xae } };
// {2DF3B196-7B4F-4EE0-9C12-2358A57BDCC6}
constexpr GUID guid_cfg_session_open =
    { 0x2df3b196, 0x7b4f, 0x4ee0, { 0x9c, 0x12, 0x23, 0x58, 0xa5, 0x7b, 0xdc, 0xc6 } };

cfg_var_modern::cfg_string g_cfg_epoch(guid_cfg_epoch, "");
cfg_var_modern::cfg_int g_cfg_revision(guid_cfg_revision, 0);
cfg_var_modern::cfg_bool g_cfg_session_open(guid_cfg_session_open, false);

struct Session {
    std::string epoch;
    ChangeLog log;
};

std::optional<Session> g_session;

// 修订号只在正常退出时写回。中途崩溃时 sessionOpen 仍为真，下次启动换 epoch（见 Resume）。
class LibrarySyncInitQuit : public initquit {
public:
    void on_quit() override {
        if (!g_session) return;
        g_cfg_revision = static_cast<int64_t>(g_session->log.Revision());
        g_cfg_session_open = false;
    }
};

initquit_factory_t<LibrarySyncInitQuit> g_library_sync_initquit;

}  // namespace

void OnLibraryLoaded() {
    if (g_session) return;

    const int64_t storedRevision = g_cfg_revision.get();
    Stored stored;
    stored.epoch = g_cfg_epoch.get().c_str();
    stored.revision = storedRevision > 0 ? static_cast<std::uint64_t>(storedRevision) : 0;
    stored.sessionOpen = g_cfg_session_open.get();

    const Stored next = Resume(stored, pfc::print_guid(pfc::createGUID()).c_str());
    if (next.epoch != stored.epoch) {
        g_cfg_epoch = next.epoch.c_str();
        g_cfg_revision = static_cast<int64_t>(next.revision);
    }
    g_cfg_session_open = true;
    // 不在事务里的写入会提交，但不保证已写到磁盘。之后崩溃时下次启动必须读到 sessionOpen，
    // 所以等它落盘再开始记录。
    fb2k::configStore::get()->commitBlocking();

    g_session.emplace(Session{next.epoch, ChangeLog(next.revision)});
    g_session->log.MarkLoaded();
}

bool Recording() {
    return g_session.has_value();
}

void OnItems(const std::vector<std::string>& handles, bool removed) {
    if (g_session) g_session->log.Record(handles, removed);
}

const std::string& Epoch() {
    return g_session->epoch;
}

const ChangeLog& Log() {
    return g_session->log;
}

}  // namespace library_sync
