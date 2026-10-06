#pragma once

// ============================================
// FrontendDirectoryPolicy.h - 前端目录解析的纯决策
//
// 给定各级候选目录，选出第一个能加载的。候选目录按什么配置拼出来由
// FrontendDirectoryResolver 负责；这里只查文件系统里有没有 index.html，
// 不依赖 foobar2000 SDK，测试可以直接链接。
// ============================================

#include <string>

namespace frontend_directory_policy {

// 命中的候选级别。除 None 外，枚举顺序就是查找顺序。
enum class Source {
    // 所有候选都没有 index.html
    None,
    // 面板自己指定的模板：profile\webview-ui\<面板模板名>
    PanelTemplate,
    // 全局活动模板：profile\webview-ui\<活动模板名>
    ActiveTemplate,
    // 组件目录下的 foo_ui_webview2_resources\dist
    Bundled,
    // profile\webview-ui\default
    DefaultTemplate,
};

// 各级候选目录，不带结尾分隔符；空串表示这一级不参与。
struct Candidates {
    std::wstring panelTemplate;
    std::wstring activeTemplate;
    std::wstring bundled;
    std::wstring defaultTemplate;
};

struct Resolution {
    Source source = Source::None;
    // source 为 None 时为空
    std::wstring directory;
};

// directory 下有名为 index.html 的文件（不是同名目录）时为真；directory 为空时为假。
bool HasIndexHtml(const std::wstring& directory);

// 按 Candidates 的字段顺序返回第一个含 index.html 的目录。
// 每一级都按 index.html 判定：只有目录、没有入口文件的候选不算命中，例如组件包里
// 预建的空 dist 目录。这样活动模板缺入口文件时，才会继续落到后面的级别。
Resolution Resolve(const Candidates& candidates);

}  // namespace frontend_directory_policy
