#include "pch.h"
#include "window/MainWindow.h"
#include "window/MainWindowInternal.h"
#include "webview/WebViewHost.h"
#include "core/PreferencesPage.h"
#include "utils/I18n.h"
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
        AppendMenuW(hFileMenu, MF_STRING, 2001, TR("New Playlist", "新建播放列表"));
        AppendMenuW(hFileMenu, MF_STRING, CMD_OPEN_FILE, TR("Open...\tCtrl+O", "打开...\tCtrl+O"));
        AppendMenuW(hFileMenu, MF_STRING, 2002, TR("Add Files...", "添加文件..."));
        AppendMenuW(hFileMenu, MF_STRING, CMD_OPEN_FOLDER, TR("Add Folder...", "添加文件夹..."));
        AppendMenuW(hFileMenu, MF_STRING, 2003, TR("Add Location...", "添加位置..."));
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, 2004, TR("Save Playlist...", "保存播放列表..."));
        AppendMenuW(hFileMenu, MF_STRING, 2005, TR("Load Playlist...", "载入播放列表..."));
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, CMD_PREFERENCES, TR("Preferences...\tCtrl+P", "首选项...\tCtrl+P"));
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFileMenu, MF_STRING, 2006, TR("Exit", "退出"));
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFileMenu, TR("File", "文件"));
    }
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
    if (hEditMenu) {
        AppendMenuW(hEditMenu, MF_STRING, 1201, TR("Undo\tCtrl+Z", "撤销\tCtrl+Z"));
        AppendMenuW(hEditMenu, MF_STRING, 1202, TR("Redo\tCtrl+Y", "重做\tCtrl+Y"));
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1203, TR("Cut\tCtrl+X", "剪切\tCtrl+X"));
        AppendMenuW(hEditMenu, MF_STRING, 1204, TR("Copy\tCtrl+C", "复制\tCtrl+C"));
        AppendMenuW(hEditMenu, MF_STRING, 1205, TR("Paste\tCtrl+V", "粘贴\tCtrl+V"));
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1206, TR("Select All\tCtrl+A", "全选\tCtrl+A"));
        AppendMenuW(hEditMenu, MF_STRING, 1207, TR("Deselect All", "取消全选"));
        AppendMenuW(hEditMenu, MF_STRING, 1208, TR("Invert Selection", "反向选择"));
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1209, TR("Remove Dead Entries", "移除失效项目"));
        AppendMenuW(hEditMenu, MF_STRING, 1210, TR("Crop", "仅保留所选"));
        AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEditMenu, MF_STRING, 1211, TR("Sort", "排序"));
        AppendMenuW(hEditMenu, MF_STRING, 1212, TR("Find...\tCtrl+F", "查找...\tCtrl+F"));
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hEditMenu, TR("Edit", "编辑"));
    }
    
    // View menu
    HMENU hViewMenu = CreatePopupMenu();
    if (hViewMenu) {
        AppendMenuW(hViewMenu, MF_STRING, 3001, TR("Always on Top", "始终置顶"));
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, 3002, TR("Layout", "布局"));
        AppendMenuW(hViewMenu, MF_STRING, 3003, TR("Playlist Manager", "播放列表管理器"));
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, CMD_RELOAD, TR("Reload UI\tF5", "重新加载界面\tF5"));
        AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hViewMenu, MF_STRING, CMD_CONSOLE, TR("Console", "控制台"));
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hViewMenu, TR("View", "视图"));
    }
    
    // Playback menu
    HMENU hPlaybackMenu = CreatePopupMenu();
    if (hPlaybackMenu) {
        AppendMenuW(hPlaybackMenu, MF_STRING, 4001, TR("Play\tSpace", "播放\tSpace"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4002, TR("Pause", "暂停"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4003, TR("Stop\tV", "停止\tV"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4004, TR("Previous\tZ", "上一首\tZ"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4005, TR("Next\tB", "下一首\tB"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4006, TR("Random\tX", "随机\tX"));
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPlaybackMenu, MF_STRING, 4007, TR("Stop After Current\tShift+V", "当前音轨后停止\tShift+V"));
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        
        // Playback Order submenu
        HMENU hOrderMenu = CreatePopupMenu();
        if (hOrderMenu) {
            AppendMenuW(hOrderMenu, MF_STRING, 4101, TR("Default", "默认"));
            AppendMenuW(hOrderMenu, MF_STRING, 4102, TR("Repeat (Playlist)", "重复(播放列表)"));
            AppendMenuW(hOrderMenu, MF_STRING, 4103, TR("Repeat (Track)", "重复(音轨)"));
            AppendMenuW(hOrderMenu, MF_STRING, 4104, TR("Shuffle (Tracks)", "乱序(音轨)"));
            AppendMenuW(hOrderMenu, MF_STRING, 4105, TR("Shuffle (Albums)", "乱序(专辑)"));
            AppendMenuW(hOrderMenu, MF_STRING, 4106, TR("Shuffle (Folders)", "乱序(文件夹)"));
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hOrderMenu, TR("Order", "播放顺序"));
        }
        
        // Volume submenu
        HMENU hVolumeMenu = CreatePopupMenu();
        if (hVolumeMenu) {
            AppendMenuW(hVolumeMenu, MF_STRING, 4201, TR("Up\t+", "调高\t+"));
            AppendMenuW(hVolumeMenu, MF_STRING, 4202, TR("Down\t-", "调低\t-"));
            AppendMenuW(hVolumeMenu, MF_STRING, 4203, TR("Mute", "静音"));
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hVolumeMenu, TR("Volume", "音量"));
        }
        
        // ReplayGain submenu
        HMENU hReplayGainMenu = CreatePopupMenu();
        if (hReplayGainMenu) {
            AppendMenuW(hReplayGainMenu, MF_STRING, 4301, TR("None", "无"));
            AppendMenuW(hReplayGainMenu, MF_STRING, 4302, TR("Track Gain", "音轨增益"));
            AppendMenuW(hReplayGainMenu, MF_STRING, 4303, TR("Album Gain", "专辑增益"));
            AppendMenuW(hReplayGainMenu, MF_STRING, 4304, TR("Track Gain (Auto)", "音轨增益(自动)"));
            AppendMenuW(hPlaybackMenu, MF_STRING | MF_POPUP, (UINT_PTR)hReplayGainMenu, TR("ReplayGain", "播放增益"));
        }
        
        AppendMenuW(hPlaybackMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPlaybackMenu, MF_STRING, 4008, TR("Seek\tCtrl+G", "跳转\tCtrl+G"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4009, TR("Cursor Follows Playback", "光标跟随播放"));
        AppendMenuW(hPlaybackMenu, MF_STRING, 4010, TR("Playback Follows Cursor", "播放跟随光标"));
        
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hPlaybackMenu, TR("Playback", "播放"));
    }
    
    // Library menu
    HMENU hLibraryMenu = CreatePopupMenu();
    if (hLibraryMenu) {
        AppendMenuW(hLibraryMenu, MF_STRING, 5001, TR("Configure...", "配置..."));
        AppendMenuW(hLibraryMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hLibraryMenu, MF_STRING, 5002, TR("Rescan", "重新扇描"));
        AppendMenuW(hLibraryMenu, MF_STRING, 5003, TR("Search...\tAlt+Q", "搜索...\tAlt+Q"));
        AppendMenuW(hLibraryMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hLibraryMenu, MF_STRING, 5004, TR("Album List", "专辑列表"));
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hLibraryMenu, TR("Library", "媒体库"));
    }
    
    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    if (hHelpMenu) {
        AppendMenuW(hHelpMenu, MF_STRING, 6001, TR("Contents", "帮助目录"));
        AppendMenuW(hHelpMenu, MF_STRING, 6002, TR("Titleformatting Help", "标题格式帮助"));
        AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelpMenu, MF_STRING, 6003, TR("Components", "组件"));
        AppendMenuW(hHelpMenu, MF_STRING, 6004, TR("Online Resources", "在线资源"));
        AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelpMenu, MF_STRING, CMD_ABOUT, TR("About foobar2000...", "关于 foobar2000..."));
        AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hHelpMenu, TR("Help", "帮助"));
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    
    // Developer Tools - 仅在配置启用时显示
    if (webview_prefs::GetDevToolsEnabled()) {
        AppendMenuW(hMenu, MF_STRING, CMD_DEVTOOLS, TR("Developer Tools\tF12", "开发者工具\tF12"));
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
