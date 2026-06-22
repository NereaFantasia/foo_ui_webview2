#include "pch.h"
#include "window/MainWindow.h"
#include "window/MainWindowInternal.h"
#include "webview/WebViewHost.h"
#include "core/PreferencesPage.h"
#include <foobar2000/SDK/menu_helpers.h>
#include <shellapi.h>

using namespace mainwindow_detail;

void MainWindow::ShowContextMenu(int screenX, int screenY) {
    // 公有方法，供 API 调用
    OnContextMenu(screenX, screenY);
}

void MainWindow::OnContextMenu(int x, int y) {
    // 如果坐标为哨兵值 -1，使用当前鼠标位置（负坐标在多显示器下是合法的）
    if (x == -1 && y == -1) {
        POINT pt;
        GetCursorPos(&pt);
        x = pt.x;
        y = pt.y;
    }
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;
    
    // ============================================
    // Complete foobar2000 DUI Menu Structure
    // ============================================
    
    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    if (hFileMenu) {
        AppendMenuW(hFileMenu, MF_STRING, 2001, L"New Playlist");
        AppendMenuW(hFileMenu, MF_STRING, CMD_OPEN_FILE, L"Open...\tCtrl+O");
        AppendMenuW(hFileMenu, MF_STRING, 2002, L"Add Files...");
        AppendMenuW(hFileMenu, MF_STRING, CMD_OPEN_FOLDER, L"Add Folder...");
        AppendMenuW(hFileMenu, MF_STRING, 2003, L"Add Location...");
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, 2004, L"Save Playlist...");
        AppendMenuW(hFileMenu, MF_STRING, 2005, L"Load Playlist...");
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, CMD_PREFERENCES, L"Preferences...\tCtrl+P");
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, 2006, L"Exit");
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFileMenu, L"File");
    }
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
    if (hEditMenu) {
        AppendMenuW(hEditMenu, MF_STRING, 1201, L"Undo\tCtrl+Z");
        AppendMenuW(hEditMenu, MF_STRING, 1202, L"Redo\tCtrl+Y");
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1203, L"Cut\tCtrl+X");
        AppendMenuW(hEditMenu, MF_STRING, 1204, L"Copy\tCtrl+C");
        AppendMenuW(hEditMenu, MF_STRING, 1205, L"Paste\tCtrl+V");
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1206, L"Select All\tCtrl+A");
        AppendMenuW(hEditMenu, MF_STRING, 1207, L"Deselect All");
        AppendMenuW(hEditMenu, MF_STRING, 1208, L"Invert Selection");
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1209, L"Remove Dead Entries");
        AppendMenuW(hEditMenu, MF_STRING, 1210, L"Crop");
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1211, L"Sort");
        AppendMenuW(hEditMenu, MF_STRING, 1212, L"Find...\tCtrl+F");
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hEditMenu, L"Edit");
    }
    
    // View menu
    HMENU hViewMenu = CreatePopupMenu();
    if (hViewMenu) {
        AppendMenuW(hViewMenu, MF_STRING, 3001, L"Always on Top");
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, 3002, L"Layout");
        AppendMenuW(hViewMenu, MF_STRING, 3003, L"Playlist Manager");
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, CMD_RELOAD, L"Reload UI\tF5");
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, CMD_CONSOLE, L"Console");
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hViewMenu, L"View");
    }
    
    // Playback menu
    HMENU hPlaybackMenu = CreatePopupMenu();
    if (hPlaybackMenu) {
        AppendMenuW(hPlaybackMenu, MF_STRING, 4001, L"Play\tSpace");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4002, L"Pause");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4003, L"Stop\tV");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4004, L"Previous\tZ");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4005, L"Next\tB");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4006, L"Random\tX");
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPlaybackMenu, MF_STRING, 4007, L"Stop After Current\tShift+V");
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        
        // Playback Order submenu
        HMENU hOrderMenu = CreatePopupMenu();
        if (hOrderMenu) {
            AppendMenuW(hOrderMenu, MF_STRING, 4101, L"Default");
            AppendMenuW(hOrderMenu, MF_STRING, 4102, L"Repeat (Playlist)");
            AppendMenuW(hOrderMenu, MF_STRING, 4103, L"Repeat (Track)");
            AppendMenuW(hOrderMenu, MF_STRING, 4104, L"Shuffle (Tracks)");
            AppendMenuW(hOrderMenu, MF_STRING, 4105, L"Shuffle (Albums)");
            AppendMenuW(hOrderMenu, MF_STRING, 4106, L"Shuffle (Folders)");
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hOrderMenu, L"Order");
        }
        
        // Volume submenu
        HMENU hVolumeMenu = CreatePopupMenu();
        if (hVolumeMenu) {
            AppendMenuW(hVolumeMenu, MF_STRING, 4201, L"Up\t+");
            AppendMenuW(hVolumeMenu, MF_STRING, 4202, L"Down\t-");
            AppendMenuW(hVolumeMenu, MF_STRING, 4203, L"Mute");
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hVolumeMenu, L"Volume");
        }
        
        // ReplayGain submenu
        HMENU hReplayGainMenu = CreatePopupMenu();
        if (hReplayGainMenu) {
            AppendMenuW(hReplayGainMenu, MF_STRING, 4301, L"None");
            AppendMenuW(hReplayGainMenu, MF_STRING, 4302, L"Track Gain");
            AppendMenuW(hReplayGainMenu, MF_STRING, 4303, L"Album Gain");
            AppendMenuW(hReplayGainMenu, MF_STRING, 4304, L"Track Gain (Auto)");
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hReplayGainMenu, L"ReplayGain");
        }
        
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPlaybackMenu, MF_STRING, 4008, L"Seek\tCtrl+G");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4009, L"Cursor Follows Playback");
        AppendMenuW(hPlaybackMenu, MF_STRING, 4010, L"Playback Follows Cursor");
        
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hPlaybackMenu, L"Playback");
    }
    
    // Library menu
    HMENU hLibraryMenu = CreatePopupMenu();
    if (hLibraryMenu) {
        AppendMenuW(hLibraryMenu, MF_STRING, 5001, L"Configure...");
        AppendMenuW(hLibraryMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hLibraryMenu, MF_STRING, 5002, L"Rescan");
        AppendMenuW(hLibraryMenu, MF_STRING, 5003, L"Search...\tAlt+Q");
        AppendMenuW(hLibraryMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hLibraryMenu, MF_STRING, 5004, L"Album List");
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hLibraryMenu, L"Library");
    }
    
    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    if (hHelpMenu) {
        AppendMenuW(hHelpMenu, MF_STRING, 6001, L"Contents");
        AppendMenuW(hHelpMenu, MF_STRING, 6002, L"Titleformatting Help");
        AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelpMenu, MF_STRING, 6003, L"Components");
        AppendMenuW(hHelpMenu, MF_STRING, 6004, L"Online Resources");
        AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelpMenu, MF_STRING, CMD_ABOUT, L"About foobar2000...");
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hHelpMenu, L"Help");
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    
    // Developer Tools - 仅在配置启用时显示
    if (webview_prefs::GetDevToolsEnabled()) {
        AppendMenuW(hMenu, MF_STRING, CMD_DEVTOOLS, L"Developer Tools\tF12");
    }
    
    // Constrain menu position within window bounds
    RECT windowRect;
    GetWindowRect(hwnd_, &windowRect);
    if (x < windowRect.left) x = windowRect.left;
    if (x > windowRect.right) x = windowRect.right;
    if (y < windowRect.top) y = windowRect.top;
    if (y > windowRect.bottom) y = windowRect.bottom;
    
    // Show menu
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

// Helper function to execute mainmenu command by name using SDK's built-in lookup
bool MainWindow::ExecuteMainMenuCommand(const char* commandName) {
    GUID cmdGuid;
    if (mainmenu_commands::g_find_by_name(commandName, cmdGuid)) {
        return mainmenu_commands::g_execute(cmdGuid);
    }
    FB2K_console_print("[WebView2 UI] Command not found: ", commandName);
    return false;
}

bool MainWindow::HandleWebViewMenuCommand(WORD cmdId) {
    switch (cmdId) {
        case CMD_RELOAD:
            if (webView_ && webView_->IsReady()) {
                webView_->Reload();
            }
            return true;
        case 9999: {
            if (!webView_ || !webView_->IsReady()) {
                return true;
            }

            console::printf("[WebView2 UI] Reloading frontend due to template change...");
            std::wstring resourcesDir = GetFrontendResourcesDir();
            if (resourcesDir.empty()) {
                return true;
            }

            HRESULT hr = webView_->SetVirtualHostMapping(GetVirtualHostName(), resourcesDir);
            if (FAILED(hr)) {
                return true;
            }

            std::wstring url = std::wstring(L"https://") + GetVirtualHostName() + L"/index.html";
            webView_->Navigate(url);
            return true;
        }
        case CMD_DEVTOOLS:
            if (webView_ && webView_->IsReady()) {
                webView_->OpenDevTools();
            }
            return true;
        case CMD_CONSOLE:
            standard_commands::run_main(standard_commands::guid_main_show_console);
            return true;
        default:
            return false;
    }
}

bool MainWindow::HandleStandardMenuCommand(WORD cmdId) {
    struct StandardCommandBinding {
        WORD id;
        const GUID* guid;
    };

    static const StandardCommandBinding bindings[] = {
        {CMD_OPEN_FILE, &standard_commands::guid_main_open},
        {CMD_OPEN_FOLDER, &standard_commands::guid_main_add_directory},
        {CMD_PREFERENCES, &standard_commands::guid_main_preferences},
        {CMD_ABOUT, &standard_commands::guid_main_about},
        {2002, &standard_commands::guid_main_add_files},
        {2003, &standard_commands::guid_main_add_location},
        {2004, &standard_commands::guid_main_save_playlist},
        {2005, &standard_commands::guid_main_load_playlist},
        {2006, &standard_commands::guid_main_exit},
        {1201, &standard_commands::guid_main_playlist_undo},
        {1202, &standard_commands::guid_main_playlist_redo},
        {1206, &standard_commands::guid_main_playlist_select_all},
        {1208, &standard_commands::guid_main_playlist_sel_invert},
        {1209, &standard_commands::guid_main_remove_dead_entries},
        {1210, &standard_commands::guid_main_playlist_sel_crop},
        {1212, &standard_commands::guid_main_playlist_search},
        {3001, &standard_commands::guid_main_always_on_top},
        {4001, &standard_commands::guid_main_play},
        {4002, &standard_commands::guid_main_pause},
        {4003, &standard_commands::guid_main_stop},
        {4004, &standard_commands::guid_main_previous},
        {4005, &standard_commands::guid_main_next},
        {4006, &standard_commands::guid_main_random},
        {4007, &standard_commands::guid_main_stop_after_current},
        {4009, &standard_commands::guid_main_cursor_follows_playback},
        {4010, &standard_commands::guid_main_playback_follows_cursor},
        {4201, &standard_commands::guid_main_volume_up},
        {4202, &standard_commands::guid_main_volume_down},
        {4203, &standard_commands::guid_main_volume_mute},
        {6002, &standard_commands::guid_main_titleformat_help},
    };

    for (const auto& binding : bindings) {
        if (binding.id == cmdId) {
            standard_commands::run_main(*binding.guid);
            return true;
        }
    }

    return false;
}

bool MainWindow::HandleMainMenuLookupCommand(WORD cmdId) {
    switch (cmdId) {
        case 2001:
            return ExecuteMainMenuCommand("File/New playlist");
        case 1203:
            return ExecuteMainMenuCommand("Edit/Cut");
        case 1204:
            return ExecuteMainMenuCommand("Edit/Copy");
        case 1205:
            return ExecuteMainMenuCommand("Edit/Paste");
        case 1207:
            return ExecuteMainMenuCommand("Edit/Selection/Deselect all");
        case 1211:
            return ExecuteMainMenuCommand("Edit/Sort");
        case 3002:
            return ExecuteMainMenuCommand("View/Layout");
        case 3003:
            return ExecuteMainMenuCommand("View/Playlist Manager");
        case 4008:
            return ExecuteMainMenuCommand("Playback/Seek");
        case 4101:
            return ExecuteMainMenuCommand("Playback/Order/Default");
        case 4102:
            return ExecuteMainMenuCommand("Playback/Order/Repeat (playlist)");
        case 4103:
            return ExecuteMainMenuCommand("Playback/Order/Repeat (track)");
        case 4104:
            return ExecuteMainMenuCommand("Playback/Order/Shuffle (tracks)");
        case 4105:
            return ExecuteMainMenuCommand("Playback/Order/Shuffle (albums)");
        case 4106:
            return ExecuteMainMenuCommand("Playback/Order/Shuffle (directories)");
        case 4301:
            return ExecuteMainMenuCommand("Playback/ReplayGain/Source mode/None");
        case 4302:
            return ExecuteMainMenuCommand("Playback/ReplayGain/Source mode/Track");
        case 4303:
            return ExecuteMainMenuCommand("Playback/ReplayGain/Source mode/Album");
        case 4304:
            return ExecuteMainMenuCommand("Playback/ReplayGain/Source mode/Track/Album by playback order");
        case 6001:
            return ExecuteMainMenuCommand("Help/Contents");
        case 6003:
            return ExecuteMainMenuCommand("Help/Components");
        default:
            return false;
    }
}

bool MainWindow::HandleLibraryMenuCommand(WORD cmdId) {
    switch (cmdId) {
        case 5001:
            library_manager::get()->show_preferences();
            return true;
        case 5002:
            return ExecuteMainMenuCommand("Library/Rescan media library");
        case 5003: {
            auto api = library_search_ui::tryGet();
            if (api.is_valid()) {
                api->show("");
                return true;
            }
            return ExecuteMainMenuCommand("Library/Search");
        }
        case 5004: {
            service_enum_t<library_viewer> e;
            service_ptr_t<library_viewer> ptr;
            while (e.next(ptr)) {
                const char* name = ptr->get_name();
                if (!name || pfc::stricmp_ascii(name, "Album List") != 0) {
                    continue;
                }

                if (!ptr->have_activate()) {
                    break;
                }

                ptr->activate();
                return true;
            }
            return ExecuteMainMenuCommand("View/Album List");
        }
        default:
            return false;
    }
}

bool MainWindow::HandleHelpMenuCommand(WORD cmdId) {
    switch (cmdId) {
        case 6004: {
            auto result = ::ShellExecuteW(hwnd_, L"open", L"https://www.foobar2000.org/", nullptr, nullptr,
                           SW_SHOWNORMAL);
            return reinterpret_cast<INT_PTR>(result) > 32;
        }
        default:
            return false;
    }
}

void MainWindow::OnCommand(WORD cmdId) {
    if (HandleWebViewMenuCommand(cmdId) ||
        HandleStandardMenuCommand(cmdId) ||
        HandleMainMenuLookupCommand(cmdId) ||
        HandleLibraryMenuCommand(cmdId) ||
        HandleHelpMenuCommand(cmdId)) {
        return;
    }
}
