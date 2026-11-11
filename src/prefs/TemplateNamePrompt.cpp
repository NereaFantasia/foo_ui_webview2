// ============================================
// TemplateNamePrompt.cpp - 起名对话框，以及新建模板、打开模板文件夹的共用实现
// ============================================

#include "pch.h"
#include "prefs/TemplateNamePrompt.h"
#include "prefs/PreferencesPage.h"
#include "prefs/PreferencesTemplateName.h"
#include "utils/I18n.h"
#include <foobar2000/SDK/coreDarkMode.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace webview_prefs {

namespace {

constexpr int IDC_PROMPT_LABEL = 1000;
constexpr int IDC_PROMPT_EDIT = 1001;
constexpr int IDC_PROMPT_HINT = 1002;

const wchar_t* TemplateNameErrorText(prefs_template::NameError error) {
    switch (error) {
    case prefs_template::NameError::Empty:
        return TR("Enter a name.", "请输入名称。");
    case prefs_template::NameError::InvalidChars:
        return TR("Only letters, numbers, hyphens and underscores are allowed.",
                  "只能使用字母、数字、连字符和下划线。");
    case prefs_template::NameError::Duplicate:
        return TR("A template with this name already exists (names are not case-sensitive).",
                  "已存在同名模板（名称不区分大小写）。");
    case prefs_template::NameError::None:
        break;
    }
    return L"";
}

void InitTemplateNameDialog(HWND hDlg, const TemplateNamePrompt& prompt) {
    SetWindowTextW(hDlg, prompt.title);
    SetDlgItemTextW(hDlg, IDC_PROMPT_LABEL, prompt.label.c_str());
    SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, prompt.hint.c_str());
    // 按钮文案在此设置：DLGITEMTEMPLATE 的 titleArray 是定长内联数组，
    // 放不下更长的译文，故模板里只留 ASCII 初始文本。
    SetDlgItemTextW(hDlg, IDOK, prompt.actionLabel);
    SetDlgItemTextW(hDlg, IDCANCEL, TR("Cancel", "取消"));
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    if (hEdit) {
        SetWindowTextW(hEdit, prompt.value.c_str());
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);
    }
}

// 操作按钮：校验通过才关对话框；不通过就把原因写进提示行、焦点留在编辑框，不落盘。
void AcceptTemplateNameIfValid(HWND hDlg, TemplateNamePrompt& prompt) {
    HWND hEdit = GetDlgItem(hDlg, IDC_PROMPT_EDIT);
    const int length = hEdit ? GetWindowTextLengthW(hEdit) : 0;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if (hEdit) GetWindowTextW(hEdit, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    
    const std::string candidate =
        prefs_template::Trim(pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr());
    const prefs_template::NameError error = prefs_template::CheckName(candidate, prompt.existing);
    if (error != prefs_template::NameError::None) {
        SetDlgItemTextW(hDlg, IDC_PROMPT_HINT, TemplateNameErrorText(error));
        if (hEdit) {
            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);
        }
        return;
    }
    prompt.accepted = candidate;
    EndDialog(hDlg, IDOK);
}

// 对话框状态：输入输出的提示，加深色模式钩子。钩子要跟对话框同寿，所以放在 PromptTemplateName 的栈上。
struct PromptDialogState {
    TemplateNamePrompt* prompt = nullptr;
    fb2k::CCoreDarkModeHooks darkMode;
};

INT_PTR CALLBACK TemplateNameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PromptDialogState*>(GetWindowLongPtrW(hDlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG:
        state = reinterpret_cast<PromptDialogState*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, static_cast<LONG_PTR>(lParam));
        InitTemplateNameDialog(hDlg, *state->prompt);
        state->darkMode.AddDialogWithControls(hDlg);   // 跟随 foobar2000 的深浅色
        return FALSE;  // 焦点已在 InitTemplateNameDialog 里交给编辑框
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && state) {
            AcceptTemplateNameIfValid(hDlg, *state->prompt);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// Win32 没有现成的“带输入框的消息框”：TaskDialog 不支持文本输入，
// 因此在内存中构建最小 DLGTEMPLATE。布局（对话框单位）：
//   [说明标签] / [编辑框] / [规则提示或错误原因] / [操作] [取消]
// DS_SETFONT 指定 MS Shell Dlg，系统会映射到当前 UI 字体，各控件尺寸随之按 DPI 换算。
struct alignas(DWORD) TemplateNameDialogTemplate {
    DLGTEMPLATE tmpl;
    WORD menuArray[1];      // 无菜单
    WORD classArray[1];     // 默认类
    WORD titleArray[1];     // 空标题，WM_INITDIALOG 再设
    WORD pointSize;
    WCHAR typeface[13];     // "MS Shell Dlg" + nul
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } label;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0081 = EDIT
        WORD titleArray[1];
        WORD extraBytes;
    } edit;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0082 = STATIC
        WORD titleArray[1];
        WORD extraBytes;
    } hint;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2]; // 0xFFFF, 0x0080 = BUTTON
        WCHAR titleArray[3]; // "OK" + nul
        WORD extraBytes;
    } ok;
    struct alignas(DWORD) {
        DLGITEMTEMPLATE item;
        WORD classArray[2];
        WCHAR titleArray[7]; // "Cancel" + nul
        WORD extraBytes;
    } cancel;
};

TemplateNameDialogTemplate BuildTemplateNameDialogTemplate() {
    TemplateNameDialogTemplate dlg = {};
    dlg.tmpl.style = DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg.tmpl.cdit = 5;
    dlg.tmpl.cx = 260; dlg.tmpl.cy = 96;
    dlg.pointSize = 8;
    wcscpy_s(dlg.typeface, L"MS Shell Dlg");
    
    dlg.label.item = {WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 7, 7, 246, 10, IDC_PROMPT_LABEL};
    dlg.label.classArray[0] = 0xFFFF; dlg.label.classArray[1] = 0x0082;
    
    dlg.edit.item = {WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 7, 20, 246, 14,
                     IDC_PROMPT_EDIT};
    dlg.edit.classArray[0] = 0xFFFF; dlg.edit.classArray[1] = 0x0081;
    
    // 提示行给三行高度：错误原因与“未加载布局不迁移”的说明在英文下也放得下。
    dlg.hint.item = {WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 7, 38, 246, 30, IDC_PROMPT_HINT};
    dlg.hint.classArray[0] = 0xFFFF; dlg.hint.classArray[1] = 0x0082;
    
    dlg.ok.item = {WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 149, 75, 50, 14, IDOK};
    dlg.ok.classArray[0] = 0xFFFF; dlg.ok.classArray[1] = 0x0080;
    wcscpy_s(dlg.ok.titleArray, L"OK");
    
    dlg.cancel.item = {WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, 203, 75, 50, 14, IDCANCEL};
    dlg.cancel.classArray[0] = 0xFFFF; dlg.cancel.classArray[1] = 0x0080;
    wcscpy_s(dlg.cancel.titleArray, L"Cancel");
    return dlg;
}

std::wstring Utf8ToWide(const std::string& text) {
    return pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
}

}  // namespace

// Create 与 Rename 共用的起名对话框。返回 true 且 prompt.accepted 为通过校验的名字；
// 取消或关闭返回 false。模态期间会重入消息循环，调用方要自己持有实例引用。
bool PromptTemplateName(HWND owner, TemplateNamePrompt& prompt) {
    TemplateNameDialogTemplate dlg = BuildTemplateNameDialogTemplate();
    PromptDialogState state;
    state.prompt = &prompt;
    const INT_PTR result = DialogBoxIndirectParamW(nullptr, &dlg.tmpl, owner, TemplateNameDlgProc,
                                                   reinterpret_cast<LPARAM>(&state));
    return result == IDOK && !prompt.accepted.empty();
}

void FillCreateTemplatePrompt(TemplateNamePrompt& prompt) {
    prompt.title = TR("Create Template", "新建模板");
    prompt.actionLabel = TR("Create", "新建");
    prompt.label = TR("Name for the new template:", "新模板的名称：");
    prompt.hint = TR("Letters, numbers, hyphens and underscores only. A folder with an index.html is created "
                     "under webview-ui.",
                     "只能使用字母、数字、连字符和下划线。将在 webview-ui 下创建带 index.html 的文件夹。");
    prompt.existing = GetTemplateList();
}

bool CreateTemplateWithReport(const std::string& name, std::wstring& failure) {
    if (CreateTemplate(name)) return true;

    failure = TR("Failed to create the template \"", "新建模板 “");
    failure += Utf8ToWide(name);
    failure += TR("\".", "” 失败。");
    if (TemplateExists(name)) {
        failure += TR("\n\nThe folder was created but index.html could not be written. "
                      "The folder is left in place:\n",
                      "\n\n文件夹已创建，但 index.html 未能写入。文件夹保留在：\n");
        failure += GetWebResourcesBaseDir() + L"\\" + Utf8ToWide(name);
    } else {
        failure += TR("\n\nThe folder could not be created. Check the permissions of the webview-ui folder.",
                      "\n\n无法创建文件夹。请检查 webview-ui 文件夹的权限。");
    }
    return false;
}

void OpenFolderPath(HWND owner, const std::wstring& path) {
    // 只打开存在的目录，不替用户建；按钮在目录缺失时已禁用，这里是兜底(fallback)。
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        MessageBoxW(owner, TR("The template folder does not exist.", "模板文件夹不存在。"),
            L"WebView2 UI", MB_OK | MB_ICONINFORMATION);
        return;
    }
    ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenTemplateFolder(HWND owner, const std::string& name) {
    std::wstring path = GetWebResourcesBaseDir();
    if (!name.empty()) {
        path += L"\\";
        path += Utf8ToWide(name);
    }
    OpenFolderPath(owner, path);
}

}  // namespace webview_prefs
