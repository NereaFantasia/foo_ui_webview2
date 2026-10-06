#pragma once
#include <functional>
#include <optional>

// Forward declaration
namespace pfc { class event_std; }

namespace fb2k {
	// In contexts where proper abortable sleep isn't available, it's acceptable to poll aborter object every <interval> seconds instead.
	static constexpr double abortPollInterval = 0.1;
}


namespace foobar2000_io {

PFC_DECLARE_EXCEPTION(exception_aborted,pfc::exception,"User abort");

typedef pfc::eventHandle_t abort_callback_event;

//! This class is used to signal underlying worker code whether user has decided to abort a potentially time-consuming operation. \n
//! It is commonly required by all filesystem related or decoding-related operations. \n
//! Code that receives an abort_callback object should periodically check it and abort any operations being performed if it is signaled, typically throwing exception_aborted. \n
//! See abort_callback_impl for an implementation.
class NOVTABLE abort_callback
{
public:
	//! Returns whether user has requested the operation to be aborted.
	virtual bool is_aborting() const = 0;
    //! Retrieves event object that can be used with some OS calls. The even object becomes signaled when abort is triggered. On win32, this is equivalent to win32 event handle (see: `CreateEvent`). \n
    //! You must not close this handle or call any methods that change this handle's state (`SetEvent()` or `ResetEvent()`), you can only wait for it.
    virtual abort_callback_event get_abort_event() const = 0;

	inline bool is_set() const {return is_aborting();}
    inline bool get() const { return is_aborting();}


	inline abort_callback_event get_handle() const {return get_abort_event();}
	
	//! Checks if user has requested the operation to be aborted, and throws @c exception_aborted if so.
	void check() const;

	//! For compatibility with old code. Do not call.
	inline void check_e() const {check();}

	
	//! Sleeps @c p_timeout_seconds or less when aborted, throws @c exception_aborted on abort.
	void sleep(double p_timeout_seconds) const;
	//! Sleeps @c p_timeout_seconds or less when aborted, returns true when execution should continue, false when not.
	bool sleep_ex(double p_timeout_seconds) const;
	bool sleepNoThrow(double p_timeout_seconds) const { return sleep_ex(p_timeout_seconds); }
    
	//! Waits for an event. Returns true if event is now signaled, false if the specified period has elapsed and the event did not become signaled. \n
	//! Throws @c exception_aborted if aborted.
    bool waitForEvent( pfc::eventHandle_t evtHandle, double timeOut ) const;
	//! Waits for an event. Returns true if event is now signaled, false if the specified period has elapsed and the event did not become signaled. \n
	//! Throws @c exception_aborted if aborted.
	bool waitForEvent(pfc::event& evt, double timeOut) const;
	bool waitForEvent(pfc::event_std& evt, double timeOut) const;

	//! Waits for an event. Returns once the event became signaled; throw @c exception_aborted if abort occurred first.
	void waitForEvent(pfc::eventHandle_t evtHandle) const;
	//! Waits for an event. Returns once the event became signaled; throw @c exception_aborted if abort occurred first.
	void waitForEvent(pfc::event& evt) const;

	bool waitForEventNoThrow(pfc::eventHandle_t evt) const;
	bool waitForEventNoThrow(pfc::event& evt) const;

	abort_callback( const abort_callback & ) = delete;
	void operator=( const abort_callback & ) = delete;
protected:
	abort_callback() {}
	~abort_callback() {}
};

} // foobar2000_io


/*
================================================================================================
 2026-06 abort_callback update
================================================================================================
 Origin
 The abort_callback ABI was frozen with foobar2000 v0.9, which makes it close to 20 years old now.
 At that time, it didn't seem necessary to include anything but a bool switch and a win32 event handle.
 At that time, it didn't seem like ABI compatibility would last two decades,
 or that non Windows ports would eventually emerge based on the same design.
================================================================================================
 Revised abort_callback aims to provide functionality similar to C++20 std::stop_token,
 while remaining ABI-compatible with existing components.
 Because there's no way to determine supported API methods by pure abort_callback interface,
 fb2k::abort_notify_manager was introduced, which keeps track of modern abort_callback instances,
 forwards modern API calls to them if possible, uses most efficient fallback implementation
 of these features otherwise.
================================================================================================
 Do not use any classes starting with underscore, or fb2k::abort_notify_manager directly.
 Use fb2k::abort_notify to receive offband notifications about your task being aborted.
 fb2k::abort_notify can be safely used in old foobar2000 versions or against abort_callback objects
 handed by legacy components - less efficient fallbacks will be used to implement notifications.
================================================================================================
 Note that these classes are now semantically similar to C++20 std library classes:
 abort_callback - std::stop_token
 abort_callback_impl - std::stop_source
   passing abort_callback& around makes it read-only, can only request stop on abort_callback_impl
 fb2k::abort_notify - std::stop_callback
================================================================================================
*/

namespace foobar2000_io {
	class _abort_notify {
	public:
		virtual void on_abort() noexcept = 0;

		_abort_notify(const _abort_notify&) = delete; void operator=(const _abort_notify&) = delete;
	protected:
		_abort_notify() {} ~_abort_notify() {}
	};
	class _abort_extended {
	public:
		virtual void add_notify(_abort_notify*) = 0;
		virtual void remove_notify(_abort_notify*) = 0;
	};
	//! \since 2.26 2026-06-24
	class abort_notify_manager : public service_base {
		FB2K_MAKE_SERVICE_COREAPI(abort_notify_manager);
	public:
		virtual void add_object(abort_callback*, _abort_extended*) noexcept = 0;
		virtual void remove_object(abort_callback*) noexcept = 0;

		virtual void add_notify(abort_callback*, _abort_notify*) noexcept = 0;
		virtual void remove_notify(abort_callback*, _abort_notify*) noexcept = 0;
	};
	
	class abort_notify : public _abort_notify { public:
		abort_notify(abort_callback & a_, std::function<void()> fn_) noexcept;
		~abort_notify() noexcept;
	protected:
		void on_abort() noexcept override;
	private:
		abort_callback & a; const std::function<void()> fn;
		bool m_registered = false;
		std::optional<pfc::thread2> m_fallback;
		std::optional<pfc::event> m_stopFallback;
	};
}

// alternate names for these objects
using foobar2000_io::abort_callback;
using foobar2000_io::abort_callback_event;
using foobar2000_io::abort_notify;

namespace fb2k {
    using foobar2000_io::abort_callback;
    using foobar2000_io::abort_callback_event;
    using foobar2000_io::abort_notify;
}

#include "abort_callback_impl.h"
