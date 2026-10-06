#pragma once

namespace fb2k {
	//! \since 2.26
	struct skipTrackParam {
		metadb_handle_ptr track;
		size_t playlist, index;
	};
	//! \since 2.26
	class skipTrack : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(skipTrack)
	public:
		//! @returns true if the track should be played, false if not
		virtual bool testTrack(const skipTrackParam&) = 0;
	};
}
