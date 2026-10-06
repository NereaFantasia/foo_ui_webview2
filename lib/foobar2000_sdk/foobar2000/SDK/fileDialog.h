#pragma once

#include <functional>

namespace fb2k {

    typedef std::function<void (arrayRef) > fileDialogReply_t;
    typedef std::function<void (stringRef) > fileDialogGetPath_t;

	class NOVTABLE fileDialogNotify : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE( fileDialogNotify, service_base );
	public:
        //! Called when user has cancelled the dialog.
		virtual void dialogCancelled() = 0;
        //! Called when the user has dismissed the dialog having selected some content.
        //! @param items Array of fsItemBase objects or strings, depending on the platform. Should accept either form. Typically, file dialogs will handle fsItems but Add Location will handle path strings. Special case: playlist format chooser sends chosen format name as a string (one array item).
		virtual void dialogOK2( arrayRef items ) = 0;

		static fileDialogNotify::ptr create( fileDialogReply_t recv );
	};

	class NOVTABLE fileDialogSetup : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE( fileDialogSetup, service_base );
	public:
		
		virtual void setTitle( const char * title ) = 0;
		virtual void setAllowsMultiple(bool bValue) = 0;
		//! Sets allowed file types - in uGetOpenFileName format, eg. "Crash logs|*.txt"
		virtual void setFileTypes( const char * fileTypeStr ) = 0;
		virtual void setDefaultType( uint32_t indexInList ) = 0;
		//! Helper, calls setFileTypes() with a mask matching all known file types
		void setAudioFileTypes();
		//! Sets default extension, dot-less
		virtual void setDefaultExtension( const char * defaultExt ) = 0;
		virtual void setInitialDirectory( const char * initDirectory ) = 0;

		virtual void setInitialValue( const char * initValue ) = 0;

        virtual void setParent(fb2k::hwnd_t wndParent) = 0;


		enum {
			locNotSet = 0,
			locComputer,
			locDownloads,
			locMusic,
			locDocuments,
			locPictures,
			locVideos,
		};
		virtual void setInitialLocation(unsigned identifier) = 0;

		//! Runs the dialog. \n
		//! The dialog may run synchronously (block run() and the whole app UI) or asynchronously, depending on the platform. \n
		//! For an example, on Windows most filedialogs work synchronously while on OSX all of them work asynchronously.
		//! @param notify Notify object invoked upon dialog completion.
		virtual void run(fileDialogNotify::ptr notify) = 0;
        
        //! Helper, creates fileDialogNotify for you.
        void run (fileDialogReply_t reply);
        void runSimple (fileDialogGetPath_t reply);
	};

	//! \since 2.25
	class NOVTABLE fileDialogSetup2 : public fileDialogSetup {
		FB2K_MAKE_SERVICE_INTERFACE(fileDialogSetup2, fileDialogSetup);
	public:
		//! Reserved for future use, returns true if understood, false if not available.
		virtual bool setParam(const char* key, const char* value) = 0;
		//! Runs a blocking modal loop, returns result conforming to fileDialogNotify::dialogOK2() semantics. \n
		//! Use only when necessary. May not be implemented for specific dialogs, throwing pfc::exception_not_implemented.
		virtual arrayRef runModal() = 0;
	};

	class NOVTABLE fileDialog : public service_base {
		FB2K_MAKE_SERVICE_COREAPI( fileDialog );
	public:
		virtual fileDialogSetup::ptr setupOpen() = 0;
		virtual fileDialogSetup::ptr setupSave() = 0;
		virtual fileDialogSetup::ptr setupOpenFolder() = 0;
		virtual fileDialogSetup::ptr setupOpenURL() = 0;
		virtual fileDialogSetup::ptr setupChoosePlaylistFormat() = 0;

		static fb2k::stringRef resultPath(fb2k::objRef);
		static pfc::string resultNativePath(fb2k::objRef);
	};

#ifdef _WIN32
	//! Drop-in replacement for shared.dll uBrowseForFolder()
	bool browseForFolder(HWND parent, const char * title, pfc::string_base & inOut);
	//! Drop-in replacement for shared.dll uGetOpenFileName()
	bool getOpenFileName(HWND parent, const char* p_ext_mask, unsigned def_ext_mask, const char* p_def_ext, const char* p_title, const char* p_directory, pfc::string_base& p_filename, BOOL b_save);
#endif // _WIN32
};
