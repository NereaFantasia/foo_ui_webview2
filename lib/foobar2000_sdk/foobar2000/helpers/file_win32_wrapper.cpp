#include "StdAfx.h"

#ifdef _WIN32

#include "file_win32_wrapper.h"

namespace file_win32_helpers {
	t_filesize get_size(HANDLE p_handle) {
		LARGE_INTEGER v = {};
		WIN32_IO_OP(GetFileSizeEx(p_handle, &v));
		return (t_filesize)v.QuadPart;
	}
	t_filesize getPosition(HANDLE p_handle) {
		LARGE_INTEGER i = {}, ret = {};
		SetLastError(NO_ERROR);
		if (!SetFilePointerEx(p_handle, i, &ret, SEEK_CUR)) exception_io_from_win32(GetLastError());
		return (t_filesize)ret.QuadPart;
	}
	void seek(HANDLE p_handle, t_filesize p_position) {
		seek(p_handle, (t_sfilesize)p_position, file::seek_from_beginning);
	}
	void seek(HANDLE p_handle,t_sfilesize p_position,file::t_seek_mode p_mode) {
		union  {
			t_int64 temp64;
			struct {
				DWORD temp_lo;
				LONG temp_hi;
			};
		};

		temp64 = p_position;
		SetLastError(ERROR_SUCCESS);		
		temp_lo = SetFilePointer(p_handle,temp_lo,&temp_hi,(DWORD)p_mode);
		if (GetLastError() != ERROR_SUCCESS) exception_io_from_win32(GetLastError());
	}

	void fillOverlapped(OVERLAPPED & ol, HANDLE myEvent, t_filesize s) {
		ol.hEvent = myEvent;
		ol.Offset = (DWORD)( s & 0xFFFFFFFF );
		ol.OffsetHigh = (DWORD)(s >> 32);
	}

	void writeOverlappedPass(HANDLE handle, HANDLE myEvent, t_filesize position, const void * in,DWORD inBytes, abort_callback & abort) {
		abort.check();
		if (inBytes == 0) return;
		OVERLAPPED ol = {};
		fillOverlapped(ol, myEvent, position);
		ResetEvent(myEvent);
		DWORD bytesWritten;
		SetLastError(NO_ERROR);
		if (WriteFile( handle, in, inBytes, &bytesWritten, &ol)) {
			// succeeded already?
			if (bytesWritten != inBytes) throw exception_io();
			return;
		}
		
		{
			const DWORD code = GetLastError();
			if (code != ERROR_IO_PENDING) exception_io_from_win32(code);
		}
		const HANDLE handles[] = {myEvent, abort.get_abort_event()};
		SetLastError(NO_ERROR);
		DWORD state = WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE);
		if (state == WAIT_OBJECT_0) {
			try {
				WIN32_IO_OP( GetOverlappedResult(handle,&ol,&bytesWritten,TRUE) );
			} catch(...) {
				CancelIo(handle);
				throw;
			}
			if (bytesWritten != inBytes) throw exception_io();
			return;
		}
		CancelIo(handle);
		throw exception_aborted();
	}

	void writeOverlapped(HANDLE handle, HANDLE myEvent, t_filesize & position, const void * in, size_t inBytes, abort_callback & abort) {
		enum {writeMAX = 16*1024*1024};
		size_t done = 0;
		while(done < inBytes) {
			size_t delta = inBytes - done;
			if (delta > writeMAX) delta = writeMAX;
			writeOverlappedPass(handle, myEvent, position, (const BYTE*)in + done, (DWORD) delta, abort);
			done += delta;
			position += delta;
		}
	}
	void writeStreamOverlapped(HANDLE handle, HANDLE myEvent, const void * in, size_t inBytes, abort_callback & abort) {
		enum {writeMAX = 16*1024*1024};
		size_t done = 0;
		while(done < inBytes) {
			size_t delta = inBytes - done;
			if (delta > writeMAX) delta = writeMAX;
			writeOverlappedPass(handle, myEvent, 0, (const BYTE*)in + done, (DWORD) delta, abort);
			done += delta;
		}
	}

	DWORD readOverlappedPass(HANDLE handle, HANDLE myEvent, t_filesize position, void * out, DWORD outBytes, abort_callback & abort) {
		abort.check();
		if (outBytes == 0) return 0;
		OVERLAPPED ol = {};
		fillOverlapped(ol, myEvent, position);
		ResetEvent(myEvent);
		DWORD bytesDone;
		SetLastError(NO_ERROR);
		if (ReadFile( handle, out, outBytes, &bytesDone, &ol)) {
			// succeeded already?
			return bytesDone;
		}

		{
			const DWORD code = GetLastError();
			switch(code) {
			case ERROR_HANDLE_EOF:
			case ERROR_BROKEN_PIPE:
				return 0;
			case ERROR_IO_PENDING:
				break; // continue
			default:
				exception_io_from_win32(code);
			};
		}

		const HANDLE handles[] = {myEvent, abort.get_abort_event()};
		SetLastError(NO_ERROR);
		DWORD state = WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE);
		if (state == WAIT_OBJECT_0) {
			SetLastError(NO_ERROR);
			if (!GetOverlappedResult(handle,&ol,&bytesDone,TRUE)) {
				const DWORD code = GetLastError();
				if (code == ERROR_HANDLE_EOF || code == ERROR_BROKEN_PIPE) bytesDone = 0;
				else {
					CancelIo(handle);
					exception_io_from_win32(code);
				}
			}
			return bytesDone;
		}
		CancelIo(handle);
		throw exception_aborted();
	}
	size_t readOverlapped(HANDLE handle, HANDLE myEvent, t_filesize & position, void * out, size_t outBytes, abort_callback & abort) {
		enum {readMAX = 16*1024*1024};
		size_t done = 0;
		while(done < outBytes) {
			size_t delta = outBytes - done;
			if (delta > readMAX) delta = readMAX;
			delta = readOverlappedPass(handle, myEvent, position, (BYTE*) out + done, (DWORD) delta, abort);
			if (delta == 0) break;
			done += delta;
			position += delta;
		}
		return done;
	}

	size_t readStreamOverlapped(HANDLE handle, HANDLE myEvent, void * out, size_t outBytes, abort_callback & abort) {
		enum {readMAX = 16*1024*1024};
		size_t done = 0;
		while(done < outBytes) {
			size_t delta = outBytes - done;
			if (delta > readMAX) delta = readMAX;
			delta = readOverlappedPass(handle, myEvent, 0, (BYTE*) out + done, (DWORD) delta, abort);
			if (delta == 0) break;
			done += delta;
		}
		return done;
	}

	HANDLE createFile(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, abort_callback & abort) {
		abort.check();
		
		return CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	}

	size_t lowLevelIO(HANDLE hFile, const GUID & guid, size_t arg1, void * arg2, size_t arg2size, bool canWrite, abort_callback & abort) {
		(void)arg1;
		abort.check();
		if ( guid == file_lowLevelIO::guid_flushFileBuffers ) {
			if (!canWrite) {
				PFC_ASSERT(!"File opened for reading, not writing");
				throw exception_io_denied();
			}
			WIN32_IO_OP( ::FlushFileBuffers(hFile) );
			return 1;
		} else if ( guid == file_lowLevelIO::guid_getFileTimes ) {
			if ( arg2size == sizeof(file_lowLevelIO::filetimes_t) ) {
				if (canWrite) WIN32_IO_OP(::FlushFileBuffers(hFile));
				auto ft = reinterpret_cast<file_lowLevelIO::filetimes_t *>(arg2);
				static_assert(sizeof(t_filetimestamp) == sizeof(FILETIME), "struct sanity");
				WIN32_IO_OP( GetFileTime( hFile, (FILETIME*)&ft->creation, (FILETIME*)&ft->lastAccess, (FILETIME*)&ft->lastWrite) );
				return 1;
			}
		} else if ( guid == file_lowLevelIO::guid_setFileTimes ) {
			if (arg2size == sizeof(file_lowLevelIO::filetimes_t)) {
				if (!canWrite) {
					PFC_ASSERT(!"File opened for reading, not writing");
					throw exception_io_denied();
				}
				WIN32_IO_OP(::FlushFileBuffers(hFile));
				auto ft = reinterpret_cast<file_lowLevelIO::filetimes_t *>(arg2);
				static_assert(sizeof(t_filetimestamp) == sizeof(FILETIME), "struct sanity");
				const FILETIME * pCreation = nullptr;
				const FILETIME * pLastAccess = nullptr;
				const FILETIME * pLastWrite = nullptr;
				if ( ft->creation != filetimestamp_invalid ) pCreation = (const FILETIME*)&ft->creation;
				if ( ft->lastAccess != filetimestamp_invalid ) pLastAccess = (const FILETIME*)&ft->lastAccess;
				if ( ft->lastWrite != filetimestamp_invalid ) pLastWrite = (const FILETIME*)&ft->lastWrite;
				WIN32_IO_OP( SetFileTime(hFile, pCreation, pLastAccess, pLastWrite) );
				return 1;
			}
		}
		return 0;
	}

	t_filestats2 stats2_from_handle(HANDLE h, const wchar_t * fallbackPath, uint32_t flags, abort_callback& a) {
		a.check();
		// Sadly GetFileInformationByHandle() is UNRELIABLE with certain net shares
		BY_HANDLE_FILE_INFORMATION info = {};
		if (GetFileInformationByHandle(h, &info)) {
			return file_win32_helpers::translate_stats2(info);
		}

		a.check();
		t_filestats2 ret;
		
		// ALWAYS get size, fail if bad handle
		ret.m_size = get_size(h);

		if (flags & (stats2_timestamp | stats2_timestampCreate)) {
			static_assert(sizeof(t_filetimestamp) == sizeof(FILETIME), "struct sanity");
			FILETIME ftCreate = {}, ftWrite = {};
			if (GetFileTime(h, &ftCreate, nullptr, &ftWrite)) {
				ret.m_timestamp = make_uint64(ftWrite); ret.m_timestampCreate = make_uint64(ftCreate);
			}
		}
		if (flags & stats2_flags) {
			// No other way to get this from handle?
			if (fallbackPath != nullptr && *fallbackPath != 0) {
				DWORD attr = GetFileAttributes(fallbackPath);
				if (attr != INVALID_FILE_ATTRIBUTES) {
					attribs_from_win32(ret, attr);
				}
			}
		}
		return ret;
	}
	void attribs_from_win32(t_filestats2& out, DWORD in) {
		out.set_readonly((in & FILE_ATTRIBUTE_READONLY) != 0);
		out.set_folder((in & FILE_ATTRIBUTE_DIRECTORY) != 0);
		out.set_hidden((in & FILE_ATTRIBUTE_HIDDEN) != 0);
		out.set_system((in & FILE_ATTRIBUTE_SYSTEM) != 0);
		out.set_local();
	}


	// Seek penalty query, effectively: is this an SSD?
	// Credit:
	// https://devblogs.microsoft.com/oldnewthing/20201023-00/?p=104395
	static bool queryVolumeSeekPenalty(HANDLE hVolume, bool& out) {
		STORAGE_PROPERTY_QUERY query = {};
		query.PropertyId = StorageDeviceSeekPenaltyProperty;
		query.QueryType = PropertyStandardQuery;
		DWORD count = 1;
		DEVICE_SEEK_PENALTY_DESCRIPTOR result = {};
		if (!DeviceIoControl(hVolume, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &result, sizeof(result), &count, nullptr)) {
			return false;
		}
		out = result.IncursSeekPenalty;
		return true;
	}

	static HANDLE GetVolumeHandleForFile(PCWSTR filePath) {
		// 2025-04 fix:
		// used to WIN32_OP_D() on GetVolumePathName() & GetVolumeNameForVolumeMountPoint()
		// GetVolumeNameForVolumeMountPoint() fails hard on subst'd volumes with ERROR_NOT_A_REPARSE_POINT
		// so we just fail gracefully for such
		wchar_t volumePath[MAX_PATH] = {};
		if (!GetVolumePathName(filePath, volumePath, ARRAYSIZE(volumePath))) {
			PFC_ASSERT(!"???");
			return NULL;
		}

		wchar_t volumeName[MAX_PATH] = {};
		if (!GetVolumeNameForVolumeMountPoint(volumePath, volumeName, ARRAYSIZE(volumeName))) return NULL;

		auto length = wcslen(volumeName);
		if ( length == 0 ) {
			PFC_ASSERT(!"???");
			return NULL;
		}
		if (length && volumeName[length - 1] == L'\\') {
			volumeName[length - 1] = L'\0';
		}

		auto ret = CreateFile(volumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (ret == INVALID_HANDLE_VALUE) return NULL;
		return ret;
	}
	bool querySeekPenalty(const wchar_t* nativePath, bool& out) {
		CHandle h;
		h.Attach( GetVolumeHandleForFile( nativePath ) );
		if (!h) return false;
		return queryVolumeSeekPenalty(h, out);
	}
	bool querySeekPenalty(const char* fb2k_path, bool& out) {
		const char * path = fb2k_path;
		if ( matchProtocol(path, "file")) path = afterProtocol(path);
		return querySeekPenalty(pfc::wideFromUTF8(path), out);
	}

	t_filestats2 common_stats() {
		t_filestats2 ret;
		ret.set_local();
		return ret;
	}
	t_filestats2 common_stats_file() {
		auto ret = common_stats(); ret.set_file(); return ret;
	}
}

#endif // _WIN32

