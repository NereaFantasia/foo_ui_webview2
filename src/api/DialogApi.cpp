// DialogApi.cpp - System Dialog API
// Provides file open/save dialogs, folder selection, and confirmation dialogs

#include "pch.h"
#include "api/DialogApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/TypedApi.h"
#include "api/generated/DialogSchema.h"
#include "utils/I18n.h"
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

    DialogFilterData ParseFilterSpecs(const std::optional<std::vector<api::dialog::FileFilter>>& filters) {
        DialogFilterData data;
        if (filters) {
            for (const auto& filter : *filters) {
                data.names.push_back(Utf8ToWide(filter.name.value_or(TRU("Files", "文件"))));

                std::wstring pattern;
                if (filter.extensions) {
                    for (size_t i = 0; i < filter.extensions->size(); i++) {
                        if (i > 0) pattern += L";";
                        const std::string& ext = (*filter.extensions)[i];
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
            data.names.emplace_back(TR("All Files", "所有文件"));
            data.patterns.emplace_back(L"*.*");
            data.specs.push_back({ data.names[0].c_str(), data.patterns[0].c_str() });
        }
        return data;
    }

    /*
     * 用 SetFolder 指定本次对话框的初始目录，避免 SetDefaultFolder 被最近使用
     * 目录覆盖。空值、解析失败或非文件夹输入不改变系统选择的目录，也不报错。
     * 这里只展开 %music%，不展开其他变量，不把传入路径写入日志。
     */
    void ApplyInitialFolder(IFileDialog* dialog, const std::string& defaultPath) {
        if (defaultPath.empty()) {
            return;
        }

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
        HRESULT hr = SHCreateItemFromParsingName(expandedPath.c_str(), nullptr, IID_IShellItem,
            reinterpret_cast<void**>(&pFolder));
        if (FAILED(hr)) {
            return;
        }

        // 初始位置必须是文件夹；文件项与属性查询失败均保持系统选择的目录。
        if (SFGAOF attrs = 0; SUCCEEDED(pFolder->GetAttributes(SFGAO_FOLDER, &attrs)) && (attrs & SFGAO_FOLDER) != 0) {
            // 返回值不检查：SetFolder 失败时对话框仍以默认位置打开，与解析失败同一结果。
            dialog->SetFolder(pFolder);
        }
        pFolder->Release();
    }
    
    //==========================================================================
    // dialog.openFile - Open file selection dialog
    //==========================================================================
    api::Result<api::dialog::OpenFileResult> DialogOpenFile(const api::dialog::OpenFileParams& p) {
        std::string title = p.title.value_or(TRU("Open File", "打开文件"));
        bool multiple = p.multiple;
        const std::string& defaultPath = p.defaultPath;

        auto filterData = ParseFilterSpecs(p.filters);
        
        api::dialog::OpenFileResult result;
        result.canceled = true;
        
        // Create file dialog
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        if (FAILED(hr)) {
            return api::Fail("Failed to initialize file dialog", ApiErrorCode::OPERATION_FAILED);
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
        
        ApplyInitialFolder(pFileOpen, defaultPath);
        
        // Show dialog — guard clause 展平结果处理嵌套
        HWND hwnd = GetMainWindowHandle();
        hr = pFileOpen->Show(hwnd);
        
        if (FAILED(hr)) {
            pFileOpen->Release();
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                return api::Fail("Dialog failed", ApiErrorCode::OPERATION_FAILED);
            }
            return result;
        }
        
        result.canceled = false;
        
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
                        result.filePaths.push_back(WideToUtf8(pszFilePath));
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
                    result.filePaths.push_back(WideToUtf8(pszFilePath));
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
    api::Result<api::dialog::SaveFileResult> DialogSaveFile(const api::dialog::SaveFileParams& p) {
        std::string title = p.title.value_or(TRU("Save File", "保存文件"));
        const std::string& defaultName = p.defaultName;

        auto filterData = ParseFilterSpecs(p.filters);
        
        api::dialog::SaveFileResult result;
        result.canceled = true;
        
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
            IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
        
        if (FAILED(hr)) {
            return api::Fail("Failed to initialize save dialog", ApiErrorCode::OPERATION_FAILED);
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
            pFileSave->Release();
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                return api::Fail("Dialog failed", ApiErrorCode::OPERATION_FAILED);
            }
            return result;
        }
        result.canceled = false;
        
        IShellItem* pItem = nullptr;
        hr = pFileSave->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result.filePath = WideToUtf8(pszFilePath);
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
    api::Result<api::dialog::OpenFolderResult> DialogOpenFolder(const api::dialog::OpenFolderParams& p) {
        std::string title = p.title.value_or(TRU("Select Folder", "选择文件夹"));
        const std::string& defaultPath = p.defaultPath;
        
        api::dialog::OpenFolderResult result;
        result.canceled = true;
        
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        if (FAILED(hr)) {
            return api::Fail("Failed to initialize folder dialog", ApiErrorCode::OPERATION_FAILED);
        }
        
        DWORD dwFlags;
        pFileOpen->GetOptions(&dwFlags);
        dwFlags |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM;
        pFileOpen->SetOptions(dwFlags);
        
        pFileOpen->SetTitle(Utf8ToWide(title).c_str());
        
        ApplyInitialFolder(pFileOpen, defaultPath);
        
        HWND hwnd = GetMainWindowHandle();
        hr = pFileOpen->Show(hwnd);
        
        if (FAILED(hr)) {
            pFileOpen->Release();
            if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                return api::Fail("Dialog failed", ApiErrorCode::OPERATION_FAILED);
            }
            return result;
        }
        
        result.canceled = false;
        
        IShellItem* pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFolderPath = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
            if (SUCCEEDED(hr)) {
                result.folderPath = WideToUtf8(pszFolderPath);
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
    api::Result<api::dialog::ConfirmResult> DialogConfirm(const api::dialog::ConfirmParams& p) {
        std::string title = p.title.value_or(TRU("Confirm", "确认"));
        const std::string& message = p.message;
        const std::string& type = p.type;
        int defaultButton = static_cast<int>(p.defaultButton);
        
        // Get button labels
        std::vector<std::wstring> buttons;
        if (p.buttons) {
            for (const std::string& btn : *p.buttons) {
                buttons.push_back(Utf8ToWide(btn));
            }
        }
        
        if (buttons.empty()) {
            buttons.emplace_back(TR("OK", "确定"));
            buttons.emplace_back(TR("Cancel", "取消"));
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
        
        api::dialog::ConfirmResult result;
        if (SUCCEEDED(hr)) {
            result.response = nClickedButton - 100;
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
                    result.response = 0;
                    break;
                case IDNO:
                    result.response = 1;
                    break;
                case IDCANCEL:
                    result.response = static_cast<std::int64_t>(buttons.size()) - 1;
                    break;
                default:
                    result.response = -1;
            }
        }
        
        return result;
    }
    
} // anonymous namespace

//==========================================================================
// Register Dialog API
//==========================================================================
// Parameters come from src/api/schema/dialog.ts through the generated types.
void RegisterDialogApi() {
    // dialog.openFile - Open file selection dialog
    api::RegisterApi("dialog.openFile", DialogOpenFile);

    // dialog.saveFile - Save file dialog
    api::RegisterApi("dialog.saveFile", DialogSaveFile);

    // dialog.openFolder - Folder selection dialog
    api::RegisterApi("dialog.openFolder", DialogOpenFolder);

    // dialog.confirm - Confirmation dialog
    api::RegisterApi("dialog.confirm", DialogConfirm);
    
    LOG("Dialog API registered (4 APIs)");
}
