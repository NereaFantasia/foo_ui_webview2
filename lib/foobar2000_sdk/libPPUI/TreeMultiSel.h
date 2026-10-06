#pragma once

// ================================================================================
// CTreeMultiSel
// Implementation of multi-selection in a tree view.
// Usage: 
// Method 1 : CTreeMultiSel 
//  Forward messages from both your dialog and tree control, to msgMapDialog / msgMapTreeView slots
// Method 2: CTreeMultiSelShim
//  Shims your dialog and control automatically, no message forwarding necessary
// In both cases, use .Setup(tree) to initialize
// Control ID of the tree MUST be valid for receiving TVN_* notifications
// ================================================================================

#include <unordered_set>
#include <vector>
#include <optional>

class CTreeMultiSel : public CMessageMap {
public:
	static constexpr UINT msgMapDialog = 0, msgMapTreeView = 1;

	void Setup(CWindow tree) {
		m_ID = GetDlgCtrlID(tree);
		assert(m_ID != 0);
		m_tree = tree;
	}
	
	CTreeMultiSel() {}

	typedef std::unordered_set<HTREEITEM> selection_t;
	typedef std::vector<HTREEITEM> selectionOrdered_t;

	BEGIN_MSG_MAP_EX(CTreeMultiSel)
		NOTIFY_HANDLER_EX(m_ID, TVN_ITEMEXPANDED, OnItemExpanded)
		NOTIFY_HANDLER_EX(m_ID, NM_CLICK, OnClick)
		NOTIFY_HANDLER_EX(m_ID, TVN_DELETEITEM, OnItemDeleted)
		NOTIFY_HANDLER_EX(m_ID, TVN_SELCHANGING, OnSelChanging)
		NOTIFY_HANDLER_EX(m_ID, TVN_SELCHANGED, OnSelChangedFilter)
		NOTIFY_HANDLER_EX(m_ID, NM_SETFOCUS, OnFocus)
		NOTIFY_HANDLER_EX(m_ID, NM_KILLFOCUS, OnFocus)
		NOTIFY_HANDLER_EX(m_ID, NM_CUSTOMDRAW, OnCustomDraw)
	ALT_MSG_MAP(msgMapTreeView)
		MSG_WM_LBUTTONDOWN(OnLButtonDown)
		MSG_WM_KEYDOWN(OnKeyDown)
		MSG_WM_CHAR(OnChar)
	END_MSG_MAP()

	// Retrieves selected items - on order of appearance in the view
	selectionOrdered_t GetSelectionOrdered(CTreeViewCtrl tree) const {
		HTREEITEM first = tree.GetRootItem();
		selectionOrdered_t ret; ret.reserve( m_selection.size() );
		for(HTREEITEM walk = first; walk != NULL; walk = tree.GetNextVisibleItem(walk)) {
			if (m_selection.contains(walk)) ret.push_back( walk );
		}
		return ret;
	}

	//! Undefined order! Use only when order of selected items is not relevant.
	selection_t GetSelection() const { return m_selection; }
	selection_t const & GetSelectionRef() const { return m_selection; }
	bool IsItemSelected(HTREEITEM item) const {return m_selection.count(item) > 0;}
	size_t GetSelCount() const {return m_selection.size();}
	//! Retrieves a single-selection item. Null if nothing or more than one item is selected.
	HTREEITEM GetSingleSel() const {
		if (m_selection.size() != 1) return NULL;
		return *m_selection.begin();
	}

	void OnContextMenu_FixSelection(CPoint pt) {
		if (pt != CPoint(-1, -1)) {
			WIN32_OP_D(m_tree.ScreenToClient(&pt));
			UINT flags = 0;
			const HTREEITEM item = m_tree.HitTest(pt, &flags);
			if (item != NULL && (flags & TVHT_ONITEM) != 0) {
				if (!IsItemSelected(item)) {					
					SelectSingleItem(item);
				}
				CallSelectItem(item);
			}
		}
	}

	void OnLButtonDown(UINT nFlags, CPoint point) {
		if (!IsKeyPressed(VK_CONTROL)) {
			UINT flags = 0;
			HTREEITEM item = m_tree.HitTest(point, &flags);
			if (item != NULL && (flags & TVHT_ONITEM) != 0) {
				if (!IsItemSelected(item)) m_tree.SelectItem(item);
			}
		}
		SetMsgHandled(FALSE);
	}
	static bool IsNavKey(UINT vk) {
		switch(vk) {
			case VK_UP:
			case VK_DOWN:
			case VK_RIGHT:
			case VK_LEFT:
			case VK_PRIOR:
			case VK_NEXT:
			case VK_HOME:
			case VK_END:
				return true;
			default:
				return false;
		}
	}
	void OnChar(TCHAR chChar, UINT nRepCnt, UINT nFlags) {
		switch(chChar) {
			case ' ':
				if (IsKeyPressed(VK_CONTROL) || !IsTypingInProgress()) {
					HTREEITEM item = m_tree.GetSelectedItem();
					if (item != NULL) SelectToggleItem(item);
					return;
				}
				break;
		}
		m_lastTypingTime = GetTickCount64();
		SetMsgHandled(FALSE);
	}
	void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
		UINT vKey = nChar;
		if (IsNavKey(vKey)) m_lastTypingTime.reset();
		switch(vKey) {
			case VK_UP:
				if (IsKeyPressed(VK_CONTROL)) {
					HTREEITEM item = m_tree.GetSelectedItem();
					if (item != NULL) {
						HTREEITEM prev = m_tree.GetPrevVisibleItem(item);
						if (prev != NULL) {
							CallSelectItem(prev);
							if (IsKeyPressed(VK_SHIFT)) {
								if (m_selStart == NULL) m_selStart = item;
								SelectItemRange(prev);
							}
						}
					}
					return;
				}
				break;
			case VK_DOWN:
				if (IsKeyPressed(VK_CONTROL)) {
					HTREEITEM item = m_tree.GetSelectedItem();
					if (item != NULL) {
						HTREEITEM next = m_tree.GetNextVisibleItem(item);
						if (next != NULL) {
							CallSelectItem(next);
							if (IsKeyPressed(VK_SHIFT)) {
								if (m_selStart == NULL) m_selStart = item;
								SelectItemRange(next);
							}
						}
					}
					return;
				}
				break;
			/*case VK_LEFT:
				if (IsKeyPressed(VK_CONTROL)) {
					tree.SendMessage(WM_HSCROLL, SB_LINEUP, 0);
				}
				break;
			case VK_RIGHT:
				if (IsKeyPressed(VK_CONTROL)) {
					tree.SendMessage(WM_HSCROLL, SB_LINEDOWN, 0);
				}
				break;*/
		}
		SetMsgHandled(FALSE);
	}
private:
	LRESULT OnFocus(LPNMHDR hdr) {
		if ( m_selection.size() > 100 ) {
			CTreeViewCtrl tree(hdr->hwndFrom);
			tree.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
		} else if (m_selection.size() > 0) {
			CTreeViewCtrl tree(hdr->hwndFrom);
			CRgn rgn; rgn.CreateRectRgn(0,0,0,0);
			for(auto walk : m_selection) AddToUpdateRgn(walk, rgn);
			tree.RedrawWindow(NULL, rgn, RDW_INVALIDATE | RDW_ERASE);
		}
		SetMsgHandled(FALSE);
		return 0;
	}
	void CallSelectItem(HTREEITEM item) {
		const bool was = m_ownSelChange; m_ownSelChange = true;
		m_tree.SelectItem(item);
		m_ownSelChange = was;
	}
	LRESULT OnSelChangedFilter(LPNMHDR) {
		if (m_ownSelChangeNotify) SetMsgHandled(FALSE);
		return 0;
	}
	LRESULT OnItemDeleted(LPNMHDR pnmh) {
		const HTREEITEM item = reinterpret_cast<NMTREEVIEW*>(pnmh)->itemOld.hItem;
		m_selection.erase( item );
		if (m_selStart == item) m_selStart = NULL;
		SetMsgHandled(FALSE);
		return 0;
	}
	LRESULT OnItemExpanded(LPNMHDR pnmh) {
		NMTREEVIEW * info = reinterpret_cast<NMTREEVIEW *>(pnmh);
		CTreeViewCtrl tree ( pnmh->hwndFrom );
		if ((info->itemNew.state & TVIS_EXPANDED) == 0) {
			if (DeselectChildren(info->itemNew.hItem )) {
				SendOnSelChanged();
			}
		}
		SetMsgHandled(FALSE);
		return 0;
	}
	
	void FixFocusItem(HTREEITEM item) {
		if (this->IsItemSelected(item) || m_tree.GetSelectedItem() != item) return;

		auto scope = pfc::autoToggle(m_ownSelChange, true);
		
		for(;;) {
			if (item == TVI_ROOT || item == NULL || this->IsItemSelected(item)) {
				m_tree.SelectItem(item); return;
			}
			for (auto walk = m_tree.GetPrevSiblingItem(item); walk != NULL; walk = m_tree.GetPrevSiblingItem(walk)) {
				if (this->IsItemSelected(walk)) {
					m_tree.SelectItem(walk); return;
				}
			}
			for (auto walk = m_tree.GetNextSiblingItem(item); walk != NULL; walk = m_tree.GetNextSiblingItem(walk)) {
				if (this->IsItemSelected(walk)) {
					m_tree.SelectItem(walk); return;
				}
			}
			item = m_tree.GetParentItem(item);
		}
	}

	BOOL HandleClick(CPoint pt) {
		UINT htFlags = 0;
		HTREEITEM item = m_tree.HitTest(pt, &htFlags);
		if (item != NULL && (htFlags & TVHT_ONITEM) != 0) {
			if (IsKeyPressed(VK_CONTROL)) {
				SelectToggleItem(item);
				FixFocusItem(item);
				return TRUE;
			} else if (item == m_tree.GetSelectedItem() && !IsItemSelected(item)) {
				SelectToggleItem(item);
				return TRUE;
			} else {
				//tree.SelectItem(item);
				return FALSE;
			}
		} else {
			return FALSE;
		}
	}

	LRESULT OnClick(LPNMHDR pnmh) {
		CPoint pt(GetMessagePos());
		WIN32_OP_D ( m_tree.ScreenToClient( &pt ) );
		return HandleClick(pt) ? 1 : 0;
	}

	LRESULT OnSelChanging(LPNMHDR pnmh) {
		if (!m_ownSelChange) {
			const auto info = reinterpret_cast<NMTREEVIEW *>(pnmh);
			const auto item = info->itemNew.hItem;

			if (IsTypingInProgress()) {
				SelectSingleItem(item);
			} else if (IsKeyPressed(VK_SHIFT)) {
				SelectItemRange(item);
			} else if (IsKeyPressed(VK_CONTROL)) {
				SelectToggleItem(item);
			} else {
				SelectSingleItem(item);
			}
		}
		return 0;
	}

	void SelectItemRange(HTREEITEM item) {
		if (m_selStart == NULL || m_selStart == item) {
			SelectSingleItem(item);
			return;
		}

		selection_t newSel = GrabRange(m_selStart, item );
		ApplySelection(std::move(newSel));
	}
	selection_t GrabRange(HTREEITEM item1, HTREEITEM item2) {
		selection_t range1, range2;
		HTREEITEM walk1 = item1, walk2 = item2;
		for(;;) {
			if (walk1 != NULL) {
				range1.insert( walk1 );
				if (walk1 == item2) {
					return range1;
				}
				walk1 = m_tree.GetNextVisibleItem(walk1);
			}
			if (walk2 != NULL) {
				range2.insert( walk2 );
				if (walk2 == item1) {
					return range2;
				}
				walk2 = m_tree.GetNextVisibleItem(walk2);
			}
			if (walk1 == NULL && walk2 == NULL) {
				// should not get here
				return selection_t();
			}
		}		
	}
	void SelectToggleItem(HTREEITEM item) {
		m_selStart = item;
		if ( IsItemSelected( item ) ) {
			m_selection.erase( item );
		} else {
			m_selection.insert( item );
		}
		UpdateItem(item);
	}

	LRESULT OnCustomDraw(LPNMHDR hdr) {
		NMTVCUSTOMDRAW* info = (NMTVCUSTOMDRAW*)hdr;
		switch (info->nmcd.dwDrawStage) {
		case CDDS_ITEMPREPAINT:
			// NOTE: This doesn't work all the way. Unflagging CDIS_FOCUS isn't respected, causing weird behaviors when using ctrl+cursors or unselecting items.
			if (this->IsItemSelected((HTREEITEM)info->nmcd.dwItemSpec)) {
				info->nmcd.uItemState |= CDIS_SELECTED;
			} else {
				info->nmcd.uItemState &= ~(CDIS_FOCUS | CDIS_SELECTED);
			}
			return CDRF_DODEFAULT;
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		default:
			return CDRF_DODEFAULT;
		}
	}
public:
	void SelectSingleItem(HTREEITEM item) {
		m_selStart = item;
		if (m_selection.size() == 1 && *m_selection.begin() == item) return;
		DeselectAll(); SelectItem(item);
	}

	void ApplySelection(selection_t && newSel) {
		CRgn updateRgn;
		bool changed = false;
		if (newSel.size() != m_selection.size() && newSel.size() + m_selection.size() > 100) {
			// don't bother with regions
			changed = true;
		} else {
			WIN32_OP_D(updateRgn.CreateRectRgn(0, 0, 0, 0) != NULL);
			for (auto walk : m_selection) {
				if (newSel.count(walk) == 0) {
					changed = true;
					AddToUpdateRgn(walk, updateRgn);
				}
			}
			for (auto walk : newSel) {
				if (m_selection.count(walk) == 0) {
					changed = true;
					AddToUpdateRgn(walk, updateRgn);
				}
			}
		}
		if (changed) {
			m_selection = std::move(newSel);
			m_tree.RedrawWindow(NULL, updateRgn);
			SendOnSelChanged();
		}
	}

	void DeselectItem(HTREEITEM item) {
		if (IsItemSelected(item)) {
			m_selection.erase(item); UpdateItem(item);
		}
	}
	void SelectItem(HTREEITEM item) {
		if (!IsItemSelected(item)) {
			m_selection.insert(item); UpdateItem(item);
		}
	}

	void DeselectAll() {
		if (m_selection.empty()) return;
		CRgn updateRgn; 
		if (m_selection.size() <= 100) {
			WIN32_OP_D(updateRgn.CreateRectRgn(0, 0, 0, 0) != NULL);
			for (auto walk : m_selection) AddToUpdateRgn(walk, updateRgn);
		}
		m_selection.clear();
		m_tree.RedrawWindow(NULL, updateRgn);
	}
private:
	void AddToUpdateRgn(HTREEITEM item, CRgn& updateRgn) {
		CRect rc;
		if (GetItemRect(item, rc)) {
			CRgn temp; WIN32_OP_D(temp.CreateRectRgnIndirect(rc));
			WIN32_OP_D(updateRgn.CombineRgn(temp, RGN_OR) != ERROR);
		}
	}
	bool GetItemRect(HTREEITEM item, CRect & rc) {
		return m_tree.GetItemRect(item, rc, FALSE);
	}
	void UpdateItem(HTREEITEM item) {
		CRect rc;
		if (GetItemRect(item, rc) ) {
			m_tree.RedrawWindow(rc);
		}
		SendOnSelChanged();
	}
	void SendOnSelChanged() {
		NMHDR hdr = {};
		hdr.code = TVN_SELCHANGED;
		hdr.hwndFrom = m_tree;
		hdr.idFrom = m_ID;
		const bool was = m_ownSelChangeNotify; m_ownSelChangeNotify = true;
		m_tree.GetParent().SendMessage(WM_NOTIFY, m_ID, (LPARAM) &hdr );
		m_ownSelChangeNotify = was;
	}

	bool DeselectChildren( HTREEITEM item ) {
		bool state = false;
		for(HTREEITEM walk = m_tree.GetChildItem( item ); walk != NULL; walk = m_tree.GetNextSiblingItem( walk ) ) {
			if (m_selection.erase(walk) > 0) state = true;
			if (m_selStart == walk) m_selStart = NULL;
			if (m_tree.GetItemState( walk, TVIS_EXPANDED ) ) {
				if (DeselectChildren( walk )) state = true;
			}
		}
		return state;
	}

	bool IsTypingInProgress() const {
		return m_lastTypingTime.has_value()  && (GetTickCount64() - *m_lastTypingTime < 500);
	}

	UINT m_ID = 0;
	CTreeViewCtrl m_tree;
	selection_t m_selection;
	HTREEITEM m_selStart = NULL;
	bool m_ownSelChangeNotify = false, m_ownSelChange = false;
	std::optional<ULONGLONG> m_lastTypingTime;
};

class CTreeMultiSelShim : public CTreeMultiSel {
public:
	CTreeMultiSelShim() : m_treeShim(this, msgMapTreeView), m_dialogShim(this, msgMapDialog) {}
	void Setup(CWindow tree) {
		CTreeMultiSel::Setup(tree);
		m_dialogShim.SubclassWindow(tree.GetParent());
		m_treeShim.SubclassWindow(tree);
	}
private:
	CContainedWindow m_treeShim, m_dialogShim;
};
