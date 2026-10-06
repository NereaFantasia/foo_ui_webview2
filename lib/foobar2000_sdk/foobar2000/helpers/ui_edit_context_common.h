#pragma once

class ui_edit_context_common : public ui_edit_context {
public:
    void initialize() override {}
    void shutdown() override {}

    // OVERRIDE ME
    // t_uint32 get_flags() override { return flag_searchable;}
    // GUID get_selection_type() override { return m_selType; }

    void select_all() override { }
    void select_none() override { }
    void get_selected_items(metadb_handle_list_ref ret) override { ret.remove_all(); }
    void remove_selection() override {}
    void crop_selection() override {}
    void clear() override {}
    void get_all_items(metadb_handle_list_ref ret) override { ret.remove_all(); }

    void get_selection_mask(pfc::bit_array_var& out) override { throw pfc::exception_not_implemented(); }
    void update_selection(const pfc::bit_array& mask, const pfc::bit_array& newVals) override { throw pfc::exception_not_implemented(); }
    t_size get_item_count(t_size) override { return 0; }
    metadb_handle_ptr get_item(t_size) override { throw pfc::exception_not_implemented(); }
    void get_items(metadb_handle_list_ref, pfc::bit_array const&) override { throw pfc::exception_not_implemented(); }
    bool is_item_selected(t_size) override { throw pfc::exception_not_implemented(); }
    void remove_items(pfc::bit_array const&) override { }
    void reorder_items(const t_size*, t_size) override {  }
    t_size get_selection_count(t_size) override { return 0; }

    void search() override { }

    void undo_backup() override {}
    void undo_restore() override {}
    void redo_restore() override {}

    void insert_items(t_size, metadb_handle_list_cref, pfc::bit_array const&) override { throw pfc::exception_not_implemented(); }

    t_size query_insert_mark() override { throw pfc::exception_not_implemented(); }
};
