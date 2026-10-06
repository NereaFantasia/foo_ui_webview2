#pragma once

#ifdef _WIN32
namespace pfc {

	template<typename what> static void _COM_AddRef(what * ptr) {
		if (ptr != NULL) ptr->AddRef();
	}
	template<typename what> static void _COM_Release(what * ptr) {
		if (ptr != NULL) ptr->Release();
	}

	template<class T>
	class com_ptr_t {
	public:
		typedef com_ptr_t<T> t_self;

		com_ptr_t( nullptr_t ) noexcept : m_ptr() {}

		com_ptr_t() noexcept : m_ptr() {}
		template<typename source> inline com_ptr_t(source * p_ptr) noexcept : m_ptr(p_ptr) {_COM_AddRef(m_ptr);}
		com_ptr_t(const t_self & p_source) noexcept : m_ptr(p_source.m_ptr) {_COM_AddRef(m_ptr);}
		com_ptr_t(t_self&& arg) noexcept : m_ptr(arg.detach()) {}
		template<typename source> inline com_ptr_t(const com_ptr_t<source> & p_source) noexcept : m_ptr(p_source.get_ptr()) {_COM_AddRef(m_ptr);}

		inline ~com_ptr_t() noexcept {_COM_Release(m_ptr);}
		
		inline void copy(T * p_ptr) noexcept {
			_COM_Release(m_ptr);
			m_ptr = p_ptr;
			_COM_AddRef(m_ptr);
		}

		template<typename source> inline void copy(const com_ptr_t<source> & p_source) noexcept {copy(p_source.get_ptr());}

		inline void attach(T * p_ptr) noexcept {
			_COM_Release(m_ptr);
			m_ptr = p_ptr;
		}	

		inline const t_self & operator=(const t_self & p_source) noexcept {copy(p_source); return *this;}
		inline const t_self & operator=(T* p_source) noexcept {copy(p_source); return *this;}
		inline const t_self& operator=(t_self&& arg) noexcept { attach(arg.detach()); return *this; }
		template<typename source> inline const t_self & operator=(const com_ptr_t<source> & p_source) noexcept {copy(p_source); return *this;}
		template<typename source> inline const t_self & operator=(source * p_ptr) noexcept {copy(p_ptr); return *this;}

		inline void release() noexcept {
			_COM_Release(m_ptr);
			m_ptr = NULL;
		}


		inline T* operator->() const noexcept {PFC_ASSERT(m_ptr);return m_ptr;}

		inline T* get_ptr() const noexcept {return m_ptr;}
		
		inline T* duplicate_ptr() const noexcept //should not be used ! temporary !
		{
			_COM_AddRef(m_ptr);
			return m_ptr;
		}

		inline T* detach() noexcept {
			return replace_null_t(m_ptr);
		}

		inline bool is_valid() const noexcept {return m_ptr != 0;}
		inline bool is_empty() const noexcept {return m_ptr == 0;}

		inline bool operator==(const com_ptr_t<T> & p_item) const noexcept {return m_ptr == p_item.m_ptr;}
		inline bool operator!=(const com_ptr_t<T> & p_item) const noexcept {return m_ptr != p_item.m_ptr;}
		inline bool operator>(const com_ptr_t<T> & p_item) const noexcept {return m_ptr > p_item.m_ptr;}
		inline bool operator<(const com_ptr_t<T> & p_item) const noexcept {return m_ptr < p_item.m_ptr;}

		inline static void g_swap(com_ptr_t<T> & item1, com_ptr_t<T> & item2) noexcept {
			pfc::swap_t(item1.m_ptr,item2.m_ptr);
		}

		inline T** receive_ptr() noexcept {release();return &m_ptr;}
		inline void** receive_void_ptr() noexcept {return (void**) receive_ptr();}

		inline t_self & operator<<(t_self & p_source) noexcept {attach(p_source.detach());return *this;}
		inline t_self & operator>>(t_self & p_dest) noexcept {p_dest.attach(detach());return *this;}
	private:
		T* m_ptr;
	};

}
#endif
