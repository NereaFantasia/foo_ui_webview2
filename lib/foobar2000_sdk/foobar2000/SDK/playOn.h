#pragma once

#include <vector>
#include <algorithm>

// Play On: introduced in foobar2000 v2.26.
// Allows sending of arbitrary audio stream URLs to recognized renderer devices.

namespace fb2k {
	struct playOnArg_t {
		const char* playURL;
		const char* apparentCodec; // can be null if not known
		bool showAllTargets;
	};
	//! \since 2.26
	//! Represents a device shown in Play On menu. \n
	//! See: `playOnProvider`.
	class playOnTarget : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE(playOnTarget, service_base);
	public:
		virtual stringRef getName() = 0;
		virtual GUID getGuid() = 0;
		virtual void play(playOnArg_t const &) = 0;
	};
	//! \since 2.26
	//! Allows components to send arbitrary audio streams (such as internet radio) to recognized renderer devices. \n
	//! Implemented by UPnP output, but can be extended by components. \n
	//! Extending it will cause your devices to appear in 'Play On' menu.
	class playOnProvider : public service_base {
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playOnProvider);
	public:
		virtual array_typed<playOnTarget>::ptr listTargets(playOnArg_t const &) = 0;
		virtual stringRef getName() = 0;
		virtual GUID getGuid() = 0;
	};

	typedef std::vector<playOnTarget::ptr> playOnTargets_t;
	//! Helper function to list available targets for the specified arguments.
	inline playOnTargets_t playOnTargetsOrdered(playOnArg_t const& arg) {
		auto providers = all_of<playOnProvider>();
		if (providers.size() > 1) {
			std::sort(providers.begin(), providers.end(), [](playOnProvider::ptr const& v1, playOnProvider::ptr const& v2) {
				return pfc::naturalSortCompare(v1->getName()->c_str(), v2->getName()->c_str()) < 0;
				});
		}

		playOnTargets_t ret;
		for (auto& p : providers) {
			auto work = p->listTargets(arg);
			ret.reserve(ret.size() + work->count());
			for (auto t : work->typed<playOnTarget>()) ret.push_back(t);
		}
		return ret;
	}

}