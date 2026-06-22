// DialogApi.cpp - System Dialog API
// Provides file open/save dialogs, folder selection, and confirmation dialogs

#include "pch.h"
#include "api/DialogApi.h"
#include "api/BridgeCore.h"
#include <ShObjIdl.h>
#include <ShlObj.h>
#include <comdef.h>

namespace {
    using json = nlohmann::json;
    
    // Get main window handle
    HWND GetMainWindowHandle() {
        // Try to get foobar2000 main window
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }

    // 对话框过滤器数据（生命期必须覆盖 IFileDialog::SetFileTypes 调用）
    struct DialogFilterData {
        std::vector<std::wstring> names;
        std::vector<std::wstring> patterns;
        std::vector<COMDLG_FILTERSPEC> specs;
    };

    DialogFilterData ParseFilterSpecs(const json& params) {
        DialogFilterData data;
        if (params.contains("filters") && params["filters"].is_array()) {
            for (const auto& filter : params["filters"]) {
                std::string name = filter.value("name", "Files");
                data.names.push_back(Utf8ToWide(name));

                std::wstring pattern;
                if (filter.contains("extensions") && filter["extensions"].is_array()) {
                    for (size_t i = 0; i < filter["extensions"].size(); i++) {
                        if (i > 0) pattern += L";";
                        std::string ext = filter["extensions"][i];
                        pattern += (ext == "*") ? L"*.*" : (L"*." + Utf8ToWide(ext));
                    }
                }
                data.patterns.push_back(pattern);
            }
        }

        for (size_t i = 0; i < data.names.size(); i++) {
            data.specs.push_back({ data.names[i].c_str(), data.patterns[i].c_str() });
        }

        if (data.specs.empty()) {
            data.names.emplace_back(L"All Files");
            data.patterns.emplace_back(L"*.*");
            data.specs.push_back({ data.names[0].c_str(), data.patterns[0].c_str() });
        }
        return data;
    }
    
    //==========================================================================
    // dialog.openFile - Open file selection dialog
    //==========================================================================
    json DialogOpenFile(const json& params) {
        std::string title = params.value("title", "Open File");
        bool multiple = params.value("multiple", false);
        std::string defaultPath = params.value("defaultPath", "");

        auto filterData = ParseFilterSpecs(params);
        
        json result;
        result["canceled"] = true;
        result["filePaths"] = json::array();
        
        // Create file dialog
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        if (FAILED(hr)) {
            return {{"canceled", false}, {"filePaths", json::array()}, {"error", "Failed to initialize file dialog"}};
        }
        
        // Set options
        DWORD dwFlags;
        pFileOpen->GetOptions(&dwFlags);
        dwFlags |= FOS_FORCEFILESYSTEM;
        if (multiple) {
            dwFlags |= FOS_ALLOWMULTISELECT;
        }
        pFileOpen->SetOptions(dwFlags);
        
        pFileOpen->SetTitle(Utf8ToWide(title).c_str());
        
        if (!filterData.specs.empty()) {
            pFileOpen->SetFileTypes(static_cast<UINT>(filterData.specs.size()), filterData.specs.data());
        }
        
        // Set default path
        if (!defaultPath.empty()) {
            std::wstring expandedPath = Utf8ToWide(defaultPath);
            
            if (expandedPath.find(L"%music%") != std::wstring::npos) {
                PWSTR musicPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music, 0, nullptr, &musicPath))) {
                    size_t pos = expandedPath.find(L"%music%");
                    expandedPath.replace(pos, 7, musicPath);
                    CoTaskMemFree(musicPath);
                }
            }
            
            IShellItem* pFolder = nullptr;
            hr = SHCreateItemFromParsingName(expandedPath.c_str(), nullptr, IID_IShellItem, 
                reinterpret_cast<void**>(&pFolder));
            if (SUCCEEDED(hr)) {
                pFileOpen->SetDefaultFolder(pFolder);
                pFolder->Release();
            }
        }
        
        // Show dialog — guard clause 展平结果处理嵌套
        HWND hwnd = GetMainWindowHandle();
        hr = pFileOpen->Show(hwnd);
        
        if (FAILED(hr)) {
            pFileOpen->Release();
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                result["error"] = "Dialog failed";
            }
            return result;
        }
        
        result["canceled"] = false;
        
        if (multiple) {
            IShellItemArray* pItems = nullptr;
            hr = pFileOpen->GetResults(&pItems);
            if (SUCCEEDED(hr)) {
                DWORD count = 0;
                pItems->GetCount(&count);
                for (DWORD i = 0; i < count; i++) {
                    IShellItem* pItem = nullptr;
                    if (FAILED(pItems->GetItemAt(i, &pItem))) continue;
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                        result["filePaths"].push_back(WideToUtf8(pszFilePath));
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
                pItems->Release();
            }
        } else {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    result["filePaths"].push_back(WideToUtf8(pszFilePath));
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        
        pFileOpen->Release();
        return result;
    }
    
    //==========================================================================
    // dialog.saveFile - Save file dialog
    //==========================================================================
    json DialogSaveFile(const json& params) {
        std::string title = params.value("title", "Save File");
        std::string defaultName = params.value("defaultName", "");

        auto filterData = ParseFilterSpecs(params);
        
        json result;
        result["canceled"] = true;
        result["filePath"] = "";
        
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
            IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
        
        if (FAILED(hr)) {
            return {{"canceled", false}, {"filePath", ""}, {"error", "Failed to initialize save dialog"}};
        }
        
        DWORD dwFlags;
        pFileSave->GetOptions(&dwFlags);
        dwFlags |= FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT;
        pFileSave->SetOptions(dwFlags);
        
        pFileSave->SetTitle(Utf8ToWide(title).c_str());
        
        if (!filterData.specs.empty()) {
            pFileSave->SetFileTypes(static_cast<UINT>(filterData.specs.size()), filterData.specs.data());
        }
        
        if (!defaultName.empty()) {
            pFileSave->SetFileName(Utf8ToWide(defaultName).c_str());
        }
        
        HWND hwnd = GetMainWindowHandle();
        hr = pFileSave->Show(hwnd);
        
        if (FAILED(hr)) {
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                result["error"] = "Dialog failed";
            }
            pFileSave->Release();
            return result;
        }
        result["canceled"] = false;
        
        IShellItem* pItem = nullptr;
        hr = pFileSave->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result["filePath"] = WideToUtf8(pszFilePath);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
        
        pFileSave->Release();
        return result;
    }
    
    //==========================================================================
    // dialog.openFolder - Folder selection dialog
    //==========================================================================
    json DialogOpenFolder(const json& params) {
        std::string title = params.value("title", "Select Folder");
        
        json result;
        result["canceled"] = true;
        result["folderPath"] = "";
        
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        if (FAILED(hr)) {
            return {{"canceled", false}, {"folderPath", ""}, {"error", "Failed to initialize folder dialog"}};
        }
        
        DWORD dwFlags;
        pFileOpen->GetOptions(&dwFlags);
        dwFlags |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM;
        pFileOpen->SetOptions(dwFlags);
        
        pFileOpen->SetTitle(Utf8ToWide(title).c_str());
        
        HWND hwnd = GetMainWindowHandle();
        hr = pFileOpen->Show(hwnd);
        
        if (FAILED(hr)) {
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                result["error"] = "Dialog failed";
            }
            pFileOpen->Release();
            return result;
        }
        
        result["canceled"] = false;
        
        IShellItem* pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFolderPath = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
            if (SUCCEEDED(hr)) {
                result["folderPath"] = WideToUtf8(pszFolderPath);
                CoTaskMemFree(pszFolderPath);
            }
            pItem->Release();
        }
        
        pFileOpen->Release();
        return result;
    }
    
    //==========================================================================
    // dialog.confirm - Confirmation dialog with custom buttons
    //==========================================================================
    json DialogConfirm(const json& params) {
        std::string title = params.value("title", "Confirm");
        std::string message = params.value("message", "");
        std::string type = params.value("type", "question");  // info, warning, error, question
        int defaultButton = params.value("defaultButton", 0);
        
        // Get button labels
        std::vector<std::wstring> buttons;
        if (params.contains("buttons") && params["buttons"].is_array()) {
            for (const auto& btn : params["buttons"]) {
                buttons.push_back(Utf8ToWide(btn.get<std::string>()));
            }
        }
        
        if (buttons.empty()) {
            buttons.emplace_back(L"OK");
            buttons.emplace_back(L"Cancel");
        }
        
        // Store wide strings to ensure lifetime
        std::wstring wideTitle = Utf8ToWide(title);
        std::wstring wideMessage = Utf8ToWide(message);
        
        // Use TaskDialog for modern look
        TASKDIALOGCONFIG config = {};
        config.cbSize = sizeof(config);
        config.hwndParent = GetMainWindowHandle();
        config.dwFlags = TDF_USE_COMMAND_LINKS_NO_ICON;
        config.pszWindowTitle = wideTitle.c_str();
        config.pszContent = wideMessage.c_str();
        
        // Set icon based on type
        if (type == "warning") {
            config.pszMainIcon = TD_WARNING_ICON;
        } else if (type == "error") {
            config.pszMainIcon = TD_ERROR_ICON;
        // "info" and unknown both fall back to the info icon
        } else {
            config.pszMainIcon = TD_INFORMATION_ICON;
        }
        
        // Build button array
        std::vector<TASKDIALOG_BUTTON> tdButtons;
        for (size_t i = 0; i < buttons.size(); i++) {
            TASKDIALOG_BUTTON btn;
            btn.nButtonID = static_cast<int>(100 + i);
            btn.pszButtonText = buttons[i].c_str();
            tdButtons.push_back(btn);
        }
        
        config.pButtons = tdButtons.data();
        config.cButtons = static_cast<UINT>(tdButtons.size());
        config.nDefaultButton = 100 + defaultButton;
        
        int nClickedButton = 0;
        HRESULT hr = TaskDialogIndirect(&config, &nClickedButton, nullptr, nullptr);
        
        json result;
        if (SUCCEEDED(hr)) {
            result["response"] = nClickedButton - 100;
        } else {
            // Fallback to MessageBox
            UINT mbType = MB_OKCANCEL;
            if (buttons.size() == 1) {
                mbType = MB_OK;
            } else if (buttons.size() == 3) {
                mbType = MB_YESNOCANCEL;
            }
            
            if (type == "warning") mbType |= MB_ICONWARNING;
            else if (type == "error") mbType |= MB_ICONERROR;
            else if (type == "question") mbType |= MB_ICONQUESTION;
            else mbType |= MB_ICONINFORMATION;
            
            int mbResult = MessageBoxW(GetMainWindowHandle(), 
                Utf8ToWide(message).c_str(), 
                Utf8ToWide(title).c_str(), 
                mbType);
            
            // Map MessageBox result to button index
            switch (mbResult) {
                case IDOK:
                case IDYES:
                    result["response"] = 0;
                    break;
                case IDNO:
                    result["response"] = 1;
                    break;
                case IDCANCEL:
                    result["response"] = buttons.size() - 1;
                    break;
                default:
                    result["response"] = -1;
            }
        }
        
        return result;
    }
    
} // anonymous namespace

//==========================================================================
// Register Dialog API
//==========================================================================
void RegisterDialogApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // dialog.openFile - Open file selection dialog
    bridge.RegisterApi("dialog.openFile", DialogOpenFile);
    
    // dialog.saveFile - Save file dialog
    bridge.RegisterApi("dialog.saveFile", DialogSaveFile);
    
    // dialog.openFolder - Folder selection dialog
    bridge.RegisterApi("dialog.openFolder", DialogOpenFolder);
    
    // dialog.confirm - Confirmation dialog
    bridge.RegisterApi("dialog.confirm", DialogConfirm);
    
    LOG("Dialog API registered (4 APIs)");
}
