// AppQuit.h - The one place app:beforeQuit goes out from
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#pragma once

namespace app_quit {

// Broadcast app:beforeQuit to every registered page, at most once per process.
// Called from this plugin's initquit::on_quit, which foobar2000 runs before it
// destroys the main window whatever the interface is, and again from
// WebViewUI::shutdown and background_service::Shutdown in case those run
// without it; later calls do nothing. Main thread only. Never throws: a failed
// broadcast is logged and shutdown carries on.
void AnnounceBeforeQuit() noexcept;

}  // namespace app_quit
