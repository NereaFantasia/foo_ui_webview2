// ============================================
// TemplateNamePrompt.h - 起名对话框，以及新建模板、打开模板文件夹的共用入口
// 偏好设置页与面板配置对话框共用，两处的起名规则、失败提示、打开目录的行为一致。
// ============================================

#pragma once

#include <string>
#include <vector>

namespace webview_prefs {

// 起名对话框的输入与结果。确定时把通过校验的名字写回 accepted。
struct TemplateNamePrompt {
    const wchar_t* title = L"";
    const wchar_t* actionLabel = L"";
    std::wstring label;                 // 编辑框上方的一行说明
    std::wstring hint;                  // 编辑框下方的规则提示；校验失败时换成原因
    std::wstring value;                 // 初始文本
    std::vector<std::string> existing;  // 现有模板名，用于重名判定
    std::string accepted;               // 输出：通过校验的名字（UTF-8）
};

// 起名对话框，新建与重命名共用。返回 true 且 prompt.accepted 为通过校验的名字；
// 取消或关闭返回 false。模态期间会重入消息循环，调用方要自己持有实例引用。
bool PromptTemplateName(HWND owner, TemplateNamePrompt& prompt);

// 填好「新建模板」用的提示：标题、说明、规则与现有模板名。
void FillCreateTemplatePrompt(TemplateNamePrompt& prompt);

// 创建模板。失败时 failure 是要告诉用户的话，如实说明磁盘上留下了什么，不假装回滚。
bool CreateTemplateWithReport(const std::string& name, std::wstring& failure);

// 在资源管理器里打开目录。目录不存在时提示，不替用户建。
void OpenFolderPath(HWND owner, const std::wstring& path);

// 在资源管理器里打开模板文件夹；name 为空时打开 webview-ui 根目录。
void OpenTemplateFolder(HWND owner, const std::string& name);

}  // namespace webview_prefs
