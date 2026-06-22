#pragma once
#include "pch.h"

// ============================================
// SelectionHolder - 选择持有者封装
// ============================================
// 
// 封装 ui_selection_holder，管理 WebView 面板的选择设置权限。
// 当面板获得焦点时获取 holder，丢失焦点时释放。

class SelectionHolder {
public:
    SelectionHolder() = default;
    ~SelectionHolder();
    
    // 禁止拷贝
    SelectionHolder(const SelectionHolder&) = delete;
    SelectionHolder& operator=(const SelectionHolder&) = delete;
    
    // ========== 生命周期管理 ==========
    
    // 获取选择持有者（在获得焦点时调用）
    void Acquire();
    
    // 释放选择持有者（在丢失焦点时调用）
    void Release();
    
    // 检查是否已获取
    bool IsAcquired() const { return holder_.is_valid(); }
    
    // ========== 选择设置 ==========
    
    // 设置选择（需要先 Acquire）
    // @param items 要设置的曲目列表
    // @return 是否成功
    bool SetSelection(metadb_handle_list_cref items);
    
    // 设置为播放列表选择跟踪模式
    // 当播放列表选择变化时，自动更新选择
    bool SetPlaylistSelectionTracking();
    
    // 设置为播放列表内容跟踪模式
    // 当活动播放列表或其内容变化时，自动更新选择
    bool SetPlaylistTracking();
    
private:
    ui_selection_holder::ptr holder_;
};
