#pragma once
#include "pch.h"
#include <set>
#include <mutex>
#include <chrono>

// 前向声明
class WebViewPanel;

// ============================================
// SelectionWatcher - 全局选择监听器
// ============================================
// 
// 监听 foobar2000 全局选择变化并广播 selection:changed 事件给所有 WebView 面板。
// 使用 ui_selection_callback_impl_base 实现自动注册。
//
// 单例自动注册到全局选择回调。

class SelectionWatcher : public ui_selection_callback_impl_base {
public:
    // 获取单例
    static SelectionWatcher& GetInstance();
    
    // 禁止拷贝
    SelectionWatcher(const SelectionWatcher&) = delete;
    SelectionWatcher& operator=(const SelectionWatcher&) = delete;
    
    // ========== 生命周期 ==========
    
    // 在 initquit::on_quit() 中调用，安全注销回调
    // 必须在 foobar2000 服务基础设施关闭前调用，否则
    // 静态单例析构时基类会尝试访问已销毁的服务导致崩溃
    void Shutdown();
    
    // ========== 面板注册 ==========
    
    // 注册面板（监听选择变化事件）
    void RegisterPanel(WebViewPanel* panel);
    
    // 取消注册面板
    void UnregisterPanel(WebViewPanel* panel);
    
    // 获取已注册面板数量
    size_t GetPanelCount() const;
    
protected:
    // ui_selection_callback 接口实现
    void on_selection_changed(metadb_handle_list_cref selection) override;
    
private:
    // 私有构造函数（单例模式）
    // activate=false: 延迟激活，首次注册时再激活
    SelectionWatcher();
    ~SelectionWatcher();
    
    // 广播选择变化事件
    void EmitSelectionChanged(metadb_handle_list_cref selection);
    
    // 已注册面板集合
    std::set<WebViewPanel*> panels_;
    mutable std::mutex mutex_;
    
    // ========== 节流 ==========
    static constexpr int THROTTLE_MS = 50;  // 50ms 节流
    std::chrono::steady_clock::time_point lastEmitTime_;
    bool pendingEmit_ = false;
    metadb_handle_list pendingSelection_;
};
