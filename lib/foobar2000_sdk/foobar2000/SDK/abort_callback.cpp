#include "foobar2000-sdk-pch.h"

#include "abort_callback.h"
#include <pfc/event_std.h>

void abort_callback::check() const {
    if (is_aborting()) {
        throw exception_aborted();
    }
}

void abort_callback::sleep(double p_timeout_seconds) const {
    if (!sleep_ex(p_timeout_seconds)) {
        throw exception_aborted();
    }
}

bool abort_callback::sleep_ex(double p_timeout_seconds) const {
	// return true IF NOT SET (timeout), false if set
	return !pfc::event::g_wait_for(get_abort_event(),p_timeout_seconds);
}

bool abort_callback::waitForEvent( pfc::eventHandle_t evtHandle, double timeOut ) const {
    int status = pfc::event::g_twoEventWait( this->get_abort_event(), evtHandle, timeOut );
    switch(status) {
        case 1: throw exception_aborted();
        case 2: return true;
        case 0: return false;
        default: uBugCheck();
    }
}

bool abort_callback::waitForEvent(pfc::event& evt, double timeOut) const {
	return waitForEvent(evt.get_handle(), timeOut); 
}

void abort_callback::waitForEvent(pfc::eventHandle_t evtHandle) const {
    [[maybe_unused]] bool status = waitForEvent(evtHandle, -1);
	PFC_ASSERT(status); // should never return false
}

void abort_callback::waitForEvent(pfc::event& evt) const {
    [[maybe_unused]] bool status = waitForEvent(evt, -1);
	PFC_ASSERT(status); // should never return false
}

bool abort_callback::waitForEventNoThrow(pfc::eventHandle_t evtHandle) const {
    int status = pfc::event::g_twoEventWait(this->get_abort_event(), evtHandle, -1);
    switch (status) {
    case 1: return false;
    case 2: return true;
    default: uBugCheck();
    }
}

bool abort_callback::waitForEventNoThrow(pfc::event& evt) const {
    return waitForEventNoThrow(evt.get_handle());
}

namespace fb2k {
	abort_callback_dummy noAbort;
}

bool abort_callback::waitForEvent(pfc::event_std& evt, double timeOut) const {
    check();
    if (timeOut < 0) {
        for (;;) {
            bool state = evt.wait_for(fb2k::abortPollInterval);
            check();
            if (state) return true;
        }
    } else if ( timeOut == 0 ) {
        return evt.is_set();
    } else {
        double left = timeOut;
        while(left > 0) {
            double pass = fb2k::abortPollInterval;
            if (pass > left) pass = left;
            bool state = evt.wait_for(pass);
            check();
            if (state) return true;
            left -= pass;
        }
        return false;
    }
}

abort_notify::abort_notify(abort_callback& a_, std::function<void()> fn_) noexcept : a(a_), fn(std::move(fn_))  {
    if (a.is_set()) { fn(); return; }

    {
        auto api = abort_notify_manager::tryGet();
        if (api) {api->add_notify(&a, this); m_registered = true; return; }
    }

    m_fallback.emplace(); m_stopFallback.emplace();
    m_fallback->startHere([this] {
        pfc::event::g_twoEventWait(a.get_abort_event(), m_stopFallback->get_handle(), -1 );
        if (a.is_aborting()) this->on_abort();
    });
}
abort_notify::~abort_notify() noexcept {
    if (m_registered) {
        auto api = abort_notify_manager::tryGet();
        if (api) api->remove_notify(&a, this);
    }
    if (m_stopFallback) {
        m_stopFallback->set_state(true);
    }
    if (m_fallback) {
        m_fallback->join();
    }
}
void abort_notify::on_abort() noexcept {
    fn();
}

abort_callback_impl::abort_callback_impl() noexcept {
    if (m_notifyManager) m_notifyManager->add_object(this, this);
}
abort_callback_impl::~abort_callback_impl() noexcept {
    if (m_notifyManager) m_notifyManager->remove_object(this);
}

void abort_callback_impl::set_state(bool p_state) {
    PFC_INSYNC(m_mutex);
    if (p_state == m_aborting) return;
    m_aborting = p_state;
    if (m_event) m_event->set_state(p_state);
    if (p_state) for (auto walk : m_notify) walk->on_abort();
}

abort_callback_event abort_callback_impl::get_abort_event() const noexcept {
    PFC_INSYNC(m_mutex);
    if (!m_event) m_event.emplace(m_aborting);
    return m_event->get_handle();
}
void abort_callback_impl::add_notify(foobar2000_io::_abort_notify* n) noexcept {
    PFC_INSYNC(m_mutex);
    if (m_aborting) n->on_abort();
    m_notify.insert(n);
}
void abort_callback_impl::remove_notify(foobar2000_io::_abort_notify* n) noexcept {
    PFC_INSYNC(m_mutex);
    m_notify.erase(n);
}
