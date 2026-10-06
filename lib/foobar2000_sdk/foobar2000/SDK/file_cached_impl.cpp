#include "foobar2000-sdk-pch.h"
#include "filesystem.h"
namespace {

#define FILE_CACHED_DEBUG_LOG 0

#if FILE_CACHED_DEBUG_LOG
#define LOCAL_DEBUG(...) PFC_DEBUG_PRINT("file_cached: ", __VA_ARGS__)
#else
#define LOCAL_DEBUG(...) ((void)0)
#endif


class file_cached_impl_v2 : public service_multi_inherit< stream_receive, service_multi_inherit< file_v2, service_multi_inherit< file_cached, file_lowLevelIO > > > {
public:
	enum {minBlockSize = 4096};
	enum {maxSkipSize = 128*1024};
	file_cached_impl_v2(size_t maxBlockSize) : m_maxBlockSize(maxBlockSize) {
		//m_buffer.set_size(blocksize);
	}
	size_t get_cache_block_size() override {return m_maxBlockSize;}
	void suggest_grow_cache(size_t suggestSize) override {
		if (m_maxBlockSize < suggestSize) m_maxBlockSize = suggestSize;
	}

	void initialize(service_ptr_t<file> p_base,abort_callback & p_abort) {
		m_base = p_base;
		m_can_seek = m_base->can_seek();
		_reinit(p_abort);
	}
	t_filestats2 get_stats2(uint32_t f, abort_callback& a) override {
		flush_buffer();
		return m_base->get_stats2_(f, a);
	}
	size_t lowLevelIO(const GUID & guid, size_t arg1, void * arg2, size_t arg2size, abort_callback & abort) override {
		abort.check();
		file_lowLevelIO::ptr ll;
		if ( ll &= m_base ) {
			flush_buffer();
			return ll->lowLevelIO(guid, arg1, arg2, arg2size, abort );
		}
		return 0;
	}
private:
	void _reinit(abort_callback & p_abort) {
		m_position = 0;
		
		if (m_can_seek) {
			m_position_base = m_base->get_position(p_abort);
		} else {
			m_position_base = 0;
		}

		m_size = m_base->get_size(p_abort);

		flush_buffer();
	}
public:

	t_filesize skip_(t_filesize n, abort_callback& a) {
		PFC_ASSERT( n <= SIZE_MAX );
		return this->read(nullptr, (size_t)n, a);
	}

	t_filesize skip(t_filesize p_bytes,abort_callback & p_abort) override {
		if (p_bytes > maxSkipSize) {
			const t_filesize size = get_size(p_abort);
			if (size != filesize_invalid) {
				const t_filesize position = get_position(p_abort);
				const t_filesize toskip = pfc::min_t( p_bytes, size - position );
				seek(position + toskip,p_abort);
				return toskip;
			}
		}
		return skip_(p_bytes, p_abort);
	}
	size_t receive(void* p, size_t n, abort_callback& a) override {
		LOCAL_DEBUG("Receiving bytes: ", n);
		if (n == 0) return 0;
		for (;;) {
			a.check();

			{
				const auto inBuffer = this->bufferRemaining();
				if (inBuffer > 0) {
					const auto delta = pfc::min_t<size_t>(inBuffer, n);
					if (p) memcpy(p, this->m_buffer.get_ptr() + m_bufferReadPtr, delta);
					m_bufferReadPtr += delta;
					m_position += delta;
					return delta;
				}
			}

			m_bufferState = m_bufferReadPtr = 0;

			baseSeek(m_position, a);

			m_readSize = pfc::min_t<size_t>(m_readSize << 1, this->m_maxBlockSize);
			if (m_readSize < minBlockSize) m_readSize = minBlockSize;
			if (p && n >= m_readSize) {
				LOCAL_DEBUG("Pass through receive: ", n);
				auto didRead = m_base->receive(p, n, a);
				LOCAL_DEBUG("Pass through received: ", didRead);
				m_position_base += didRead; m_position += didRead;
				return didRead;
			}
			m_buffer.grow_size(m_readSize);
			LOCAL_DEBUG("Buffer receive: ", n);
			m_bufferState = m_base->receive(m_buffer.get_ptr(), m_readSize, a);
			LOCAL_DEBUG("Buffer received: ", m_bufferState);
			if (m_bufferState == 0) return 0;
			m_position_base += m_bufferState;
		}
	}
	size_t read(void* p,size_t n,abort_callback& a) override {
		return this->read_using_receive(p,n,a);
	}

	void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) override {
		LOCAL_DEBUG("Writing bytes: ", p_bytes);
		p_abort.check();
		baseSeek(m_position,p_abort);
		m_base->write(p_buffer,p_bytes,p_abort);
		m_position_base = m_position = m_position + p_bytes;
		if (m_size < m_position) m_size = m_position;
		flush_buffer();
	}

	t_filesize get_size(abort_callback & p_abort) override {
		p_abort.check();
		return m_size;
	}
	t_filesize get_position(abort_callback & p_abort) override {
		p_abort.check();
		PFC_ASSERT( m_position <= m_size );
		return m_position;
	}
	void set_eof(abort_callback & p_abort) {
		p_abort.check();
		baseSeek(m_position,p_abort);
		m_base->set_eof(p_abort);
		flush_buffer();
	}
	void seek(t_filesize p_position,abort_callback & p_abort) override {
		LOCAL_DEBUG("Seeking: ", p_position);
		p_abort.check();
		if (!m_can_seek) throw exception_io_object_not_seekable();
		if (p_position > m_size) throw exception_io_seek_out_of_range();
		int64_t delta = p_position - m_position;

		// special case
		if (delta >= 0 && delta <= this->minBlockSize) {
			LOCAL_DEBUG("Skip-seeking: ", p_position);
			[[maybe_unused]] t_filesize skipped = this->skip_( delta, p_abort );
			PFC_ASSERT( skipped == (t_filesize)delta );
			return;
		}

		m_position = p_position;
		// within currently buffered data?
		if ((delta >= 0 && (uint64_t) delta <= bufferRemaining()) || (delta < 0 && (uint64_t)(-delta) <= m_bufferReadPtr)) {
#if FILE_CACHED_DEBUG_LOG
			FB2K_DebugLog() << "Quick-seeking: " << p_position;
#endif
			m_bufferReadPtr += (ptrdiff_t)delta;
		} else {
#if FILE_CACHED_DEBUG_LOG
			FB2K_DebugLog() << "Slow-seeking: " << p_position;
#endif
			this->flush_buffer();
		}
	}
	void reopen(abort_callback & p_abort) override {
		if (this->m_can_seek) {
			seek(0,p_abort);
		} else {
			this->m_base->reopen( p_abort );
			this->_reinit( p_abort );
		}
	}
	bool can_seek() override {return m_can_seek;}
	bool get_content_type(pfc::string_base & out) override {return m_base->get_content_type(out);}
	void on_idle(abort_callback & p_abort) override {p_abort.check();m_base->on_idle(p_abort);}
	t_filetimestamp get_timestamp(abort_callback & p_abort) override {p_abort.check(); return m_base->get_timestamp(p_abort);}
	bool is_remote() override {return m_base->is_remote();}
	void resize(t_filesize p_size,abort_callback & p_abort) override {
		flush_buffer();
		m_base->resize(p_size,p_abort);
		m_size = p_size;
		if (m_position > m_size) m_position = m_size;
		if (m_position_base > m_size) m_position_base = m_size;
	}
private:
	size_t bufferRemaining() const {return m_bufferState - m_bufferReadPtr;}
	void baseSeek(t_filesize p_target,abort_callback & p_abort) {
		if (p_target != m_position_base) {
			m_base->seek(p_target,p_abort);
			m_position_base = p_target;
		}
	}

	void flush_buffer() {
		m_bufferState = m_bufferReadPtr = 0;
		m_readSize = 0;
	}

	service_ptr_t<file> m_base;
	t_filesize m_position,m_position_base,m_size;
	bool m_can_seek;
	size_t m_bufferState, m_bufferReadPtr;
	pfc::array_t<t_uint8> m_buffer;
	size_t m_maxBlockSize;
	size_t m_readSize;
};

} // namespace

file::ptr file_cached::g_create(service_ptr_t<file> p_base,abort_callback & p_abort, t_size blockSize) {

	if (p_base->is_in_memory()) {
		return p_base; // do not want
	}

	{ // do not duplicate cache layers, check if the file we're being handed isn't already cached
		file_cached::ptr c;
		if (p_base->service_query_t(c)) {
			c->suggest_grow_cache(blockSize);
			return p_base;
		}
	}

	auto obj = fb2k::service_new< file_cached_impl_v2 >(blockSize);
	obj->initialize(p_base,p_abort);
	file_v2* asdf = obj.get_ptr();
	return asdf;
}

void file_cached::g_create(service_ptr_t<file> & p_out,service_ptr_t<file> p_base,abort_callback & p_abort, t_size blockSize) {
	p_out = g_create(p_base, p_abort, blockSize);
}

void file_cached::g_decodeInitCache(file::ptr & theFile, abort_callback & abort, size_t blockSize) {
	if (theFile->is_remote() || !theFile->can_seek()) return;

	g_create(theFile, theFile, abort, blockSize);
}

void file_cached::selftest() {
	auto & a = fb2k::noAbort;
	auto content = filesystem::g_open_tempmem();
	constexpr size_t contentSize = 1024*1024, chunkSize = 256, nChunks = contentSize/chunkSize;
	constexpr size_t cacheSize = 16 * 1024;
	
	std::vector<uint8_t> contentBuffer(contentSize), readBuffer(contentSize+1);
	for (size_t walk = 0; walk < contentSize; ++walk) {
		contentBuffer[walk] = (walk&0xFF)^((walk>>8)&0xFF)^((walk>>16)&0xFF)^((walk>>24)&0xFF);
	}

	content->write(contentBuffer.data(), contentSize, a);
	

	content->seek(0, a);

	auto cached = fb2k::service_new<file_cached_impl_v2>(cacheSize);
	cached->initialize(content, a);

	
	// read first N
	auto verify_read = [&](size_t at, size_t count) {
		if (at == SIZE_MAX) {
			at = (size_t) cached->get_position(a);
		} else {
			cached->seek(at, a);
		}
		const auto nCanRead = pfc::min_t(contentSize-at, count);
		const auto nRead = cached->read(readBuffer.data(), count, a);
		if (nRead != nCanRead) throw std::runtime_error("wrong size read");
		if (nRead > 0 && memcmp(readBuffer.data(), contentBuffer.data() + at, nRead) != 0) throw std::runtime_error("wrong content read");
	};

	verify_read(0, 1);
	verify_read(0, 2);
	verify_read(0, 3);
	verify_read(contentSize, 1);
	verify_read(0, contentSize/2);
	verify_read(SIZE_MAX, contentSize / 2 + 1);
	verify_read(0, contentSize + 1);
	verify_read(0, cacheSize);
	verify_read(SIZE_MAX, cacheSize/2);
	verify_read(SIZE_MAX, cacheSize*2);
	
	verify_read(0, 100);
	for (size_t walk = 0; walk < 100; ++walk) {
		verify_read(SIZE_MAX, 100);
	}
}