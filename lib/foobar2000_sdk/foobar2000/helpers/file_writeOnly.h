#pragma once
#include <functional>

class file_writeOnly : public file {
public:
	t_size read(void* p_buffer, t_size p_bytes, abort_callback& p_abort) { throw pfc::exception_not_implemented(); }

	t_filesize get_size(abort_callback& p_abort) { throw exception_io_object_not_seekable(); }

	t_filesize get_position(abort_callback& p_abort) { throw exception_io_object_not_seekable(); }

	void resize(t_filesize p_size, abort_callback& p_abort) { throw pfc::exception_not_implemented(); }

	void seek(t_filesize p_position, abort_callback& p_abort) { throw exception_io_object_not_seekable(); }

	void seek_ex(t_sfilesize p_position, t_seek_mode p_mode, abort_callback& p_abort) { throw exception_io_object_not_seekable(); }

	bool can_seek() { return false; }

	bool get_content_type(pfc::string_base& p_out) { return false; }

	void reopen(abort_callback& p_abort) { throw pfc::exception_not_implemented(); }

	bool is_remote() { return false; }
};

class file_writeFN : public file_writeOnly {
	t_filesize m_wrote = 0;
public:
	typedef std::function<void(const void*, size_t, abort_callback&)> fn_t;
	fn_t fn;
	void write(const void* ptr, size_t bytes, abort_callback& a) override { fn(ptr, bytes, a); m_wrote += bytes; }
	t_filesize get_size(abort_callback&) override { return m_wrote; }
	t_filesize get_position(abort_callback&) override { return m_wrote; }
};