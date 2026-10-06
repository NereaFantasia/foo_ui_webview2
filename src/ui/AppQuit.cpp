// AppQuit.cpp - The one place app:beforeQuit goes out from
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#include "pch.h"
#include "ui/AppQuit.h"
#include "api/EventEmit.h"
#include "api/generated/AppSchema.h"

#include <atomic>
#include <exception>

namespace app_quit {

namespace {
std::atomic<bool> g_announced{false};
}  // namespace

void AnnounceBeforeQuit() noexcept {
    if (g_announced.exchange(true)) return;
    try {
        // Queued to each page's WebView; the host does not wait for handlers.
        api::emit::Broadcast<api::app::events::BeforeQuit>({});
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: beforeQuit broadcast failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during beforeQuit broadcast");
    }
}

}  // namespace app_quit

namespace {

// Runs with any interface, so a page in a Default UI or Columns UI panel hears
// of the quit too. The initquit contract puts on_quit before the main window,
// and so the panels in it, are destroyed.
class AppQuitInitQuit : public initquit {
public:
    void on_quit() override { app_quit::AnnounceBeforeQuit(); }
};

initquit_factory_t<AppQuitInitQuit> g_app_quit_initquit;

}  // namespace
