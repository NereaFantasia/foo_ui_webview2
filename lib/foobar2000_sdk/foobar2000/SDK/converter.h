#pragma once
#include "event_logger.h"

namespace fb2k {
	class postConvertActionCallback {
	public:
		virtual void progress(const char * status, double progress) = 0;
		virtual event_logger::ptr loggerForTrack(size_t indexOfTrack) = 0;
	};
	//! Declares actions that can be automatically performed after conversion. \n
	//! Each action can have multiple modes (such as ReplayGain album gain vs track gain scan). \n
	//! Multiple actions can be chained, but Converter will not permit the same action to be added twice, which makes modes mutually exclusive.
	class postConvertAction : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(postConvertAction);
	public:
		//! GUID of the action, to be saved in Converter preset.
		virtual GUID getGuid() = 0;
		virtual const char* getName() = 0;
		virtual unsigned getNumModes() = 0;
		//! GUID of the mode, to be saved in Converter preset together with action GUID.
		virtual GUID getModeGuid(unsigned n) = 0;
		virtual const char* getModeName(unsigned n) = 0;
		virtual void run(const GUID & mode, metadb_handle_list_cref items, postConvertActionCallback&, abort_callback& ) = 0;
	};
}