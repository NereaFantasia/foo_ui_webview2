#pragma once
#include <optional>
#include <pfc/map.h>

class file_info_delta {
public:
	file_info_delta(const file_info& before, const file_info& after) {
		{
			auto d = after.get_length();
			if (d != before.get_length()) m_duration = d;
		}

		{
			pfc::map_t<pfc::string8, size_t, comparator> after_map;
			const size_t nMetaAfter = after.meta_get_count();
			for (size_t iMeta = 0; iMeta < nMetaAfter; ++iMeta) {
				after_map[after.meta_enum_name(iMeta)] = iMeta;
			}
			const size_t nMetaBefore = before.meta_get_count();
			for (size_t iMeta = 0; iMeta < nMetaBefore; ++iMeta) {
				const auto key = before.meta_enum_name(iMeta);
				const auto iter = after_map.find(key);
				if (!iter.is_valid()) {
					m_metaRemove += key; continue;
				}
				if (file_info::field_value_equals(before, iMeta, after, iter->m_value)) { // unchanged?
					after_map.remove(iter);
				}
			}
			for (auto& walk : after_map) {
				m_metaSet[walk.m_key] = after.meta_values(walk.m_value);
			}
		}

		{
			pfc::map_t<pfc::string8, size_t, comparator> after_map;
			const size_t nInfoAfter = after.info_get_count();
			for (size_t iInfo = 0; iInfo < nInfoAfter; ++iInfo) {
				after_map[after.info_enum_name(iInfo)] = iInfo;
			}
			const size_t nInfoBefore = before.info_get_count();
			for (size_t iInfo = 0; iInfo < nInfoBefore; ++iInfo) {
				const auto key = before.info_enum_name(iInfo);
				const auto iter = after_map.find(key);
				if (!iter.is_valid()) {
					m_infoRemove += key; continue;
				}
				if (strcmp(before.info_enum_value(iInfo), after.info_enum_value(iter->m_value)) == 0) { // unchanged?
					after_map.remove(iter);
				}
			}
			for (auto& walk : after_map) {
				m_infoSet[walk.m_key] = after.info_enum_value(walk.m_value);
			}
		}
		{
			const auto rg1 = before.get_replaygain(), rg2 = after.get_replaygain();
			if (rg1.m_track_gain != rg2.m_track_gain) m_trackGain = rg2.m_track_gain;
			if (rg1.m_album_gain != rg2.m_album_gain) m_albumGain = rg2.m_album_gain;
			if (rg1.m_track_peak != rg2.m_track_peak) m_trackPeak = rg2.m_track_peak;
			if (rg1.m_album_peak != rg2.m_album_peak) m_albumPeak = rg2.m_album_peak;
		}
	}

	void apply(file_info& to) const {
		if (m_duration) to.set_length(*m_duration);
		to.meta_remove_if([&](const char* key) {return m_metaRemove.contains(key) || m_metaSet.contains(key);});
		to.info_remove_if([&](const char* key) {return m_infoRemove.contains(key) || m_infoSet.contains(key);});
		for (auto& walk : m_metaSet) {
			PFC_ASSERT(!to.meta_exists(walk.m_key));
			size_t idx = SIZE_MAX;
			for (auto& val : walk.m_value) {
				if (idx == SIZE_MAX) idx = to.__meta_add_unsafe(walk.m_key, val);
				else to.meta_add_value(idx, val);
			}
		}
		for (auto& walk : m_infoSet) {
			PFC_ASSERT(!to.info_exists(walk.m_key));
			to.__info_add_unsafe(walk.m_key, walk.m_value);
		}
		if (m_trackGain || m_albumGain || m_trackPeak || m_albumPeak) {
			auto rg = to.get_replaygain();
			if (m_trackGain) rg.m_track_gain = *m_trackGain;
			if (m_albumGain) rg.m_album_gain = *m_albumGain;
			if (m_trackPeak) rg.m_track_peak = *m_trackPeak;
			if (m_albumPeak) rg.m_album_peak = *m_albumPeak;
			to.set_replaygain(rg);
		}
	}
private:
	std::optional<double> m_duration;
	std::optional<float> m_trackGain, m_albumGain, m_trackPeak, m_albumPeak;
	using comparator = file_info::field_name_comparator;
	pfc::set_t<pfc::string8, comparator > m_metaRemove, m_infoRemove;
	pfc::map_t<pfc::string8, pfc::array_t<pfc::string8>, comparator > m_metaSet;
	pfc::map_t<pfc::string8, pfc::string8, comparator > m_infoSet;
};
