#include "pch.h"
#include "selection/SelectionHolder.h"
#include "window/WindowChromeTrace.h"

// ============================================
// SelectionHolder Implementation
// ============================================

SelectionHolder::~SelectionHolder() {
    Release();
}

void SelectionHolder::Acquire() {
    if (holder_.is_valid()) {
        // 已经持有，不需要重复获取
        return;
    }
    
    try {
        auto selMgr = ui_selection_manager::get();
        holder_ = selMgr->acquire();
        if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("SelectionHolder: Acquired");
    } catch (...) {
        LOG("SelectionHolder: Failed to acquire");
        holder_.release();
    }
}

void SelectionHolder::Release() {
    if (holder_.is_valid()) {
        if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("SelectionHolder: Released");
        holder_.release();
    }
}

bool SelectionHolder::SetSelection(metadb_handle_list_cref items) {
    if (!holder_.is_valid()) {
        LOG("SelectionHolder: SetSelection failed - not acquired");
        return false;
    }
    
    try {
        holder_->set_selection(items);
        LOG("SelectionHolder: SetSelection - ", items.get_count(), " items");
        return true;
    } catch (...) {
        LOG("SelectionHolder: SetSelection exception");
        return false;
    }
}

bool SelectionHolder::SetPlaylistSelectionTracking() {
    if (!holder_.is_valid()) {
        LOG("SelectionHolder: SetPlaylistSelectionTracking failed - not acquired");
        return false;
    }
    
    try {
        holder_->set_playlist_selection_tracking();
        LOG("SelectionHolder: SetPlaylistSelectionTracking enabled");
        return true;
    } catch (...) {
        LOG("SelectionHolder: SetPlaylistSelectionTracking exception");
        return false;
    }
}

bool SelectionHolder::SetPlaylistTracking() {
    if (!holder_.is_valid()) {
        LOG("SelectionHolder: SetPlaylistTracking failed - not acquired");
        return false;
    }
    
    try {
        holder_->set_playlist_tracking();
        LOG("SelectionHolder: SetPlaylistTracking enabled");
        return true;
    } catch (...) {
        LOG("SelectionHolder: SetPlaylistTracking exception");
        return false;
    }
}
