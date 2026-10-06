#pragma once
#include "synchro.h"
#include <memory>
#include <vector>

namespace pfc {
	template<typename obj_t>
	class objPool {
	public:
		objPool(size_t maxCount = SIZE_MAX) : m_maxCount(maxCount) { m_pool.reserve(32); }
		typedef std::shared_ptr<obj_t> objRef_t;

		objRef_t get() {
			insync(m_sync);
			if (m_pool.empty()) return nullptr;
			auto ret = std::move(m_pool.back());
			m_pool.pop_back();
			return ret;
		}
		objRef_t make() {
			auto obj = get();
			if ( ! obj ) obj = std::make_shared<obj_t>();
			return obj;
		}
		void setMaxCount(size_t c) {
			insync(m_sync);
			m_maxCount = c;
		}
		void put(objRef_t && obj) {
			insync(m_sync); 
			if ( m_pool.size() < m_maxCount ) {
				m_pool.push_back(std::move(obj));
			}
		}
	private:
		size_t m_maxCount;
		std::vector<objRef_t> m_pool;
		critical_section m_sync;
	};

}