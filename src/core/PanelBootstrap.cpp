// WebViewPanel::RegisterAllApis 与 InitializeCallbacks 的定义单独放在这一个 TU：
// 全部 api/ 与 callbacks/ 模块头只在这里 include，面板本体的 TU 不再依赖它们。
#include "pch.h"
#include "core/WebViewPanel.h"

// API 注册函数声明
#include "api/PlaybackApi.h"
#include "api/ConfigApi.h"
#include "api/PlaylistApi.h"
#include "api/LibraryApi.h"
#include "api/WindowApi.h"
#include "api/ArtworkApi.h"
#include "api/FileApi.h"
#include "api/DialogApi.h"
#include "api/ClipboardApi.h"
#include "api/ShellApi.h"
#include "api/HttpApi.h"
#include "api/KeyboardApi.h"
#include "api/UiApi.h"
#include "api/CursorApi.h"
#include "api/LyricsApi.h"
#include "api/MetadataApi.h"
#include "api/AudioApi.h"
#include "api/MediaApi.h"
#include "api/DspApi.h"
#include "api/OutputApi.h"
#include "api/ConsoleApi.h"
#include "api/MiscApi.h"
#include "api/MenuApi.h"
#include "api/DndApi.h"
#include "api/QueueApi.h"
#include "api/DiscoveryApi.h"
#include "api/ReplayGainApi.h"
#include "api/PlaycountApi.h"
#include "api/TitleformatApi.h"
#include "api/SelectionApi.h"
#include "api/TrayApi.h"
#include "api/TaskbarApi.h"
#include "api/PortApi.h"
#include "api/WebviewApi.h"
#include "api/PluginRegistry.h"
// 回调Initializing函数声明
#include "callbacks/PlaybackCallback.h"
#include "callbacks/PlaylistCallback.h"
#include "callbacks/LibraryCallback.h"
#include "callbacks/MetadbCallback.h"

void WebViewPanel::RegisterAllApis() {
    LOG("Registering all APIs...");
    
    RegisterPlaybackApi();
    RegisterConfigApi();
    RegisterPlaylistApi();
    RegisterLibraryApi();
    RegisterWindowApi();
    RegisterArtworkApi();
    RegisterFileApi();
    RegisterDialogApi();
    RegisterClipboardApi();
    RegisterShellApi();
    RegisterHttpApi();
    RegisterKeyboardApi();
    RegisterUiApi();
    RegisterCursorApi();
    RegisterLyricsApi();
    RegisterMetadataApi();
    RegisterAudioApi();
    RegisterMediaApi();
    RegisterDspApi();
    RegisterOutputApi();
    RegisterConsoleApi();
    RegisterMiscApi();
    RegisterMenuApi();
    RegisterDndApi();
    RegisterQueueApi();
    
    // JIT Queue API
    InitializeJitQueue();
    RegisterJitQueueApi();
    
    // Plugin Registry
    PluginRegistry::GetInstance().Initialize();
    
    // Discovery API
    discovery_api::RegisterApis();
    
    // ReplayGain API
    RegisterReplayGainApi();
    
    // Playcount API
    RegisterPlaycountApi();
    
    // Titleformat API
    RegisterTitleformatApi();
    
    // Selection API
    RegisterSelectionApi();
    
    // Port/Event/State API (PortHub)
    RegisterPortApi();
    
    // Tray & Taskbar APIs
    RegisterTrayApi();
    RegisterTaskbarApi();

    // WebView page source
    RegisterWebviewApi();

    LOG("All APIs registered");
}

void WebViewPanel::InitializeCallbacks() {
    LOG("Initializing callbacks...");
    
    InitPlaybackCallbacks();
    InitPlaylistCallbacks();
    InitLibraryCallbacks();
    InitMetadbCallbacks();
    
    LOG("Callbacks initialized");
}
