#pragma once

// ============================================
// FrontendDirectoryResolver.h - 按当前配置解析前端目录
//
// 主窗口、弹出窗口、DUI/CUI 面板和面板配置对话框都经这里解析，
// 对话框里显示的目录因此就是实际加载的目录。查找顺序与判定规则见
// FrontendDirectoryPolicy.h。
// ============================================

#include <string>

#include "core/FrontendDirectoryPolicy.h"

namespace frontend_directory {

// panelTemplateName 为空表示跟随全局活动模板。
// profile\webview-ui 不存在时会顺带建出来（webview_prefs::GetWebResourcesBaseDir 的行为）；
// 建不出来时只剩组件目录这一级候选。
frontend_directory_policy::Resolution Resolve(const std::string& panelTemplateName);

// 命中那一级对应的模板名：面板模板、活动模板与 default 三级才有，其余为空串。
// 活动模板名按调用时的配置读，要与 Resolve 在同一次处理里调用才对得上。
std::string TemplateNameOf(frontend_directory_policy::Source source, const std::string& panelTemplateName);

}  // namespace frontend_directory
