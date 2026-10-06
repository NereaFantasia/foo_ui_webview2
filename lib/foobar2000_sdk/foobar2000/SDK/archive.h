#pragma once
#include <functional>
#include "filesystem.h"

namespace foobar2000_io {
	class archive;

	//! OBSOLETE, DO NOT USE OR IMPLEMENT
	class NOVTABLE archive_callback : public abort_callback {
	public:
		virtual bool on_entry(archive * owner,const char * url,const t_filestats & p_stats,const service_ptr_t<file> & p_reader) = 0;
	};

	//! Interface for archive reader services. When implementing, derive from archive_impl rather than from deriving from archive directly.
	class NOVTABLE archive : public filesystem {
		FB2K_MAKE_SERVICE_INTERFACE(archive,filesystem);
	protected:
		//! OBSOLETE, DO NOT USE OR OVERRIDE \n
		//! Call @c archive_list_() helper to list archive contents.
		virtual void archive_list(const char * p_path,const service_ptr_t<file> & p_reader,archive_callback & p_callback,bool p_want_readers) = 0;
	public:
		typedef std::function<void(const char* url, const t_filestats& stats, file::ptr reader) > list_func_t;
		//! Helper implemented on top of the other @c archive_list methods. \n
		//! This is the intended primary function for listing archive contents. \n
		//! Renamed from @c archive_list() overload in 2026-06. If you see compilation errors related to that, update your code to call @c archive_list_().
		void archive_list_(const char * path, file::ptr, list_func_t, bool wantReaders, abort_callback&);
		void archive_list_flags_(const char * path, file::ptr, list_func_t, uint32_t flags, abort_callback&);

		//! Optional method to weed out unsupported formats prior to calling @c archive_list(). \n
        //! Use this to suppress calls to @c archive_list() to avoid spurious exceptions being thrown. \n
		//! Implemented via @c archive_v2.
		bool is_our_archive( const char * path );
        
        //! Helper; extracts archive to the specified folder.
        void extract_to(const char * arc, file::ptr fileObjHint, const char * folderTo, abort_callback&);
	};

	//! \since 1.5
	//! New 1.5 series API, though allowed to implement/call in earlier versions. \n
	//! Suppresses spurious C++ exceptions on all files not recognized as archives by this instance.
	class NOVTABLE archive_v2 : public archive {
		FB2K_MAKE_SERVICE_INTERFACE(archive_v2, archive)
	public:

		//! Optional method to weed out unsupported formats prior to calling @c archive_list(). \n
        //! Use this to suppress calls to @c archive_list() to avoid spurious exceptions being thrown.
        virtual bool is_our_archive( const char * path ) = 0;

        static archive_v2::ptr tryGet(const char * path);
    };

	//! \since 1.6
	//! New 1.6 series API, though allowed to implement/call in earlier versions.
	class NOVTABLE archive_v3 : public archive_v2 {
		FB2K_MAKE_SERVICE_INTERFACE(archive_v3, archive_v2)
	public:
		//! Determine supported archive file types. \n
		//! Returns a list of extensions, colon delimited, e.g.: "zip,rar,7z"
		virtual void list_extensions(pfc::string_base & out) = 0;
	};
    //! \since 2.1
    class NOVTABLE archive_v4 : public archive_v3 {
        FB2K_MAKE_SERVICE_INTERFACE(archive_v4, archive_v3)
    public:
        virtual fb2k::arrayRef archive_list_v4( fsItemFilePtr item, file::ptr readerOptional, abort_callback & a);
    };
	//! \since 2.26
	class NOVTABLE archive_v5 : public archive_v4 {
		FB2K_MAKE_SERVICE_INTERFACE(archive_v5, archive_v4);
	public:
		class callback { 
		public:
			virtual bool on_entry(const char* url, const t_filestats2& stats, file::ptr reader) = 0;
		};
        //! Callback wants reader objects for enumerated files - that is, @c archive_list() call is being used to extract whole archive.
		static constexpr uint32_t flagReaders = 1,
        //! Combine with @c flagReaders to indicate that each reader will be actually valid ONLY for the duration of the callback, retaining it beyond that will result in undefined behavior. \n
        //! If not set, it is technically legal (but not necessarily a good idea) to retain the readers and use later. \n
        //! It's highly recommended to set it along with @c flagReaders for performance reasons, unless your implementation specifically requires them to remain valid..
            flagReadersTemporary = 2;
        
		//! Proper entry function to list archive contents, replaces @c archive::archive_list(),
		virtual void archive_list_v5(const char * path, file::ptr readerOptional, callback&, uint32_t flags, abort_callback&) = 0;
	private:
		//! Implementation of legacy method forcibly wrapped to modern. Should not be called.
		void archive_list(const char*, const service_ptr_t<file>&, archive_callback&, bool) override final;
	};

	//! Root class for archive implementations. Derive from this instead of from archive directly.
	class NOVTABLE archive_impl : public service_multi_inherit<archive_v5, filesystem_v4> {
	public:
		//do not override these
		bool get_canonical_path(const char * path,pfc::string_base & out) override final;
		bool is_our_path(const char * path) override final;
		bool get_display_path(const char * path,pfc::string_base & out) override final;
		void remove(const char * path,abort_callback & p_abort) override final;
		void move(const char * src,const char * dst,abort_callback & p_abort) override final;
		void move_overwrite(const char* src, const char* dst, abort_callback& abort) override final;
		bool is_remote(const char * src) override final;
        t_filestats2 getStatsOpportunist(const char * path) override final;
		bool relative_path_create(const char * file_path,const char * playlist_path,pfc::string_base & out) override final;
		bool relative_path_parse(const char * relative_path,const char * playlist_path,pfc::string_base & out) override final;
		void create_directory(const char * path,abort_callback &) override final;
		void make_directory(const char* path, abort_callback& abort, bool* didCreate = nullptr) override final;
		void list_directory(const char* p_path, directory_callback& p_out, abort_callback& p_abort) override final;
		void list_directory_ex(const char* p_path, directory_callback& p_out, unsigned listMode, abort_callback& p_abort) override final;
		void list_directory_v3(const char* path, directory_callback_v3& callback, unsigned listMode, abort_callback& p_abort) override final;
		t_filestats2 get_stats2(const char* p_path, uint32_t s2flags, abort_callback& p_abort) override final;
		void get_stats(const char* p_path, t_filestats& p_stats, bool& p_is_writeable, abort_callback& p_abort) override final;
		bool supports_content_types() override final { return false; }
		char pathSeparator() override final { return '/'; }
		void extract_filename_ext(const char * path, pfc::string_base & outFN) override final;
		bool get_display_name_short(const char* in, pfc::string_base& out) override final;

        // Can be overridden if more than one extension is recognized
        void list_extensions(pfc::string_base & out) override { out = get_archive_type(); }
        // Specialized in some archivhe readers
        void open(service_ptr_t<file> & p_out,const char * path, t_open_mode mode,abort_callback & p_abort) override;
protected:
		//override these
		virtual const char * get_archive_type() = 0;//eg. "zip", must be lowercase
		virtual t_filestats2 get_stats2_in_archive(const char * p_archive,const char * p_file,unsigned s2flags,abort_callback & p_abort) = 0;
		virtual void open_archive(service_ptr_t<file> & p_out,const char * archive,const char * file, abort_callback & p_abort) = 0;//opens for reading
	public:
		//override these
		// virtual void archive_list(const char * path,const service_ptr_t<file> & p_reader,archive_callback & p_out,bool p_want_readers) = 0;
		// virtual bool is_our_archive( const char * path ) = 0;
		
		static bool g_is_unpack_path(const char * path);
		static bool g_parse_unpack_path(const char * path,pfc::string_base & archive,pfc::string_base & file);
		static bool g_parse_unpack_path_ex(const char * path,pfc::string_base & archive,pfc::string_base & file, pfc::string_base & type);
		static void g_make_unpack_path(pfc::string_base & path,const char * archive,const char * file,const char * type);
		void make_unpack_path(pfc::string_base & path,const char * archive,const char * file);

		static t_filestats2 stats2_in_archive(t_filestats2 const& inArchive, t_filestats2 const& wholeArchiveStats);
		static t_filestats2 stats2_in_archive(t_filestats const& inArchive, t_filestats2 const& wholeArchiveStats);
	};

	template<typename T>
	class archive_factory_t : public service_factory_single_t<T> {};
}
