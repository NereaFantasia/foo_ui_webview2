/**
 * WebViewDuiElement - DUI 面板实现
 * 
 * 为 foobar2000 Default UI 提供 WebView2 面板支持。
 * 继承 ui_element 和 WebViewPanel。
 */

#pragma once
#include "pch.h"
#include "core/WebViewPanel.h"
#include "panels/PanelConfig.h"
#include "utils/I18n.h"

// ============================================
// GUID 定义
// ============================================

// {F8A3B2C1-5D4E-6F7A-8B9C-0D1E2F3A4B5C}
static constexpr GUID g_webview_dui_element_guid = 
    { 0xf8a3b2c1, 0x5d4e, 0x6f7a, { 0x8b, 0x9c, 0x0d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c } };

// ============================================
// WebViewDuiElementInstance - DUI 面板实例
// ============================================

class WebViewDuiElementInstance : public ui_element_instance, public WebViewPanel {
public:
    WebViewDuiElementInstance(ui_element_config::ptr cfg, ui_element_instance_callback::ptr callback);
    ~WebViewDuiElementInstance();
    
    // ========== ui_element_instance 接口实现 ==========
    
    // 返回窗口句柄 - 返回子窗口供 DUI 布局管理
    fb2k::hwnd_t get_wnd() override { return m_hostWnd; }
    
    // 配置管理
    void set_configuration(ui_element_config::ptr data) override;
    ui_element_config::ptr get_configuration() override;
    
    // GUID
    GUID get_guid() override { return g_webview_dui_element_guid; }
    GUID get_subclass() override { return ui_element_subclass_utility; }
    
    // 通知处理
    void notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size) override;
    
    // 最小/最大尺寸
    ui_element_min_max_info get_min_max_info() override;
    
    // 初始化窗口
    void Initialize(HWND parent);
    
protected:
    // ========== WebViewPanel 虚函数重写 ==========
    
    void OnWebViewReady() override;
    
private:
    // 窗口类名
    static constexpr wchar_t CLASS_NAME[] = L"WebViewDuiElement";
    
    // 注册窗口类
    static bool RegisterWindowClass();
    static bool s_classRegistered;
    
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // DUI 回调
    ui_element_instance_callback::ptr m_callback;
    
    // 注意：面板配置使用基类 WebViewPanel::panelConfig_
    
    // 宿主窗口
    HWND m_hostWnd = nullptr;
    
    // DUI 编辑模式状态
    bool m_editMode = false;
    
    // 显示配置对话框并应用结果
    void ShowConfigDialog();
    
    // 绘制编辑模式占位符
    void PaintEditModePlaceholder(HDC hdc, const RECT& rc);
};

// ============================================
// WebViewDuiElement - DUI 面板工厂
// ============================================

class WebViewDuiElement : public ui_element {
public:
    // 元素 GUID
    GUID get_guid() override { return g_webview_dui_element_guid; }
    
    // 子类 GUID (utility 类型)
    GUID get_subclass() override { return ui_element_subclass_utility; }
    
    // 元素名称
    void get_name(pfc::string_base& out) override { out = "WebView2 Panel"; }
    
    // 创建实例
    ui_element_instance_ptr instantiate(fb2k::hwnd_t parent, ui_element_config::ptr cfg, 
                                        ui_element_instance_callback_ptr callback) override;
    
    // 默认配置
    ui_element_config::ptr get_default_configuration() override;
    
    // 非容器元素，返回 NULL
    ui_element_children_enumerator_ptr enumerate_children(ui_element_config::ptr cfg) override { 
        return nullptr; 
    }
    
    // 元素描述
    bool get_description(pfc::string_base& out) override {
        out = TRU("Displays a WebView2-based UI panel for custom web interfaces.",
                  "显示用于自定义 Web 界面的 WebView2 UI 面板。");
        return true;
    }
};

