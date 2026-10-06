#pragma once
#include <unordered_set>
#include <optional>

namespace foobar2000_io {
	//! Standard implementation of @c abort_callback interface.
	class abort_callback_impl : public abort_callback, private _abort_extended {
	public:
		abort_callback_impl() noexcept;
		~abort_callback_impl() noexcept;
		abort_callback_impl(const abort_callback_impl&) = delete;
		const abort_callback_impl& operator=(const abort_callback_impl&) = delete;
		inline void abort() { set_state(true); }
		inline void set() { set_state(true); }
		inline void reset() { set_state(false); }

		void set_state(bool p_state);
		bool is_aborting() const override { return m_aborting; }

		abort_callback_event get_abort_event() const noexcept override;
	private:
		void add_notify(_abort_notify* n) noexcept override;
		void remove_notify(_abort_notify* n) noexcept override;


		volatile bool m_aborting = false;
		mutable std::optional<pfc::event> m_event;
		mutable pfc::mutex m_mutex;
		std::unordered_set<_abort_notify*> m_notify;
        const abort_notify_manager::ptr m_notifyManager = abort_notify_manager::tryGet();
	};

	//! Dummy @c abort_callback that never gets aborted. \n
	//! Note that there's no need to create instances of it, use shared @c fb2k::noAbort object instead.
	class abort_callback_dummy : public abort_callback {
	public:
		bool is_aborting() const override { return false; }
		abort_callback_event get_abort_event() const override { return m_event; }
	private:
		const abort_callback_event m_event = GetInfiniteWaitEvent();
	};
}

// Alternate names for these objects
using foobar2000_io::abort_callback_impl;
using foobar2000_io::abort_callback_dummy;

namespace fb2k {
    using foobar2000_io::abort_callback_impl;
    using foobar2000_io::abort_callback_dummy;

//! A shared @c abort_callback_dummy instance. \n
	//! Use when some function requires an @c abort_callback& and you don't have one: @c somefunc(fb2k::noAbort);
	extern abort_callback_dummy noAbort;
}
