#pragma once
class titleformat_hook_map : public titleformat_hook {
public:
	bool process_field(titleformat_text_out* p_out, const char* p_name, t_size p_name_length, bool& p_found_flag) override {
		auto iter = m_content.find(pfc::string_part_ref{ p_name, p_name_length });
		if (iter.is_empty()) return false;
		p_found_flag = iter->m_value.found;
		p_out->write(iter->m_value.type, iter->m_value.text);
		return true;
	}
	bool process_function(titleformat_text_out* p_out, const char* p_name, t_size p_name_length, titleformat_hook_function_params* p_params, bool& p_found_flag) override { return false; }

	void add_index(size_t index0based, size_t total, const char* prefix = nullptr) {
		if (index0based >= total) return;
		auto str_total = pfc::format_uint(total);
		auto str_index = pfc::format_uint(index0based + 1, (unsigned)strlen(str_total));
		if (prefix) {
			m_content[pfc::format(prefix, "_list_index")] = { str_index.c_str() };
			m_content[pfc::format(prefix, "_list_total")] = { str_total.c_str() };
		} else {
			m_content["list_index"] = { str_index.c_str() };
			m_content["list_total"] = { str_total.c_str() };
		}
	}

	struct elem_t {
		pfc::string8 text;
		GUID type = titleformat_inputtypes::unknown;
		bool found = true;
	};

	void add(const char* key, elem_t&& value) {
		m_content[key] = std::move(value);
	}

	pfc::map_t<pfc::string8, elem_t, file_info::field_name_comparator> m_content;
};