#pragma once
#include <exception>
#include <functional>

namespace fb2k {
	class once {
	public:
		void call(std::function<void()> work, abort_callback & abort) {
            for(;;) {
                std::shared_ptr<pfc::event> waitFor;
                std::exception_ptr throwMe;
                bool bProcess = false;
                bool bComplete = false;
                {
                    PFC_INSYNC(m_mutex);
                    if (m_complete) return;
                    else if (m_throwMe) throwMe = m_throwMe;
                    else if (m_started) {
                        waitFor = make_event();
                    } else {
                        // defer event creation until actual concurrent access
                        m_started = true;
                        bProcess = true;
                    }
                }
                if (throwMe) std::rethrow_exception(throwMe);
                else if (bProcess) {
                    try {
                        work();
                        bComplete = true;
                    } catch ( exception_aborted const & ) {
                    } catch (...) {
                        throwMe = std::current_exception();
                    }
                    {
                        PFC_INSYNC(m_mutex);
                        m_started = false;
                        m_throwMe = throwMe;
                        m_complete = bComplete;
                        if (m_event) {
                            m_event->set_state(true);
                            m_event = nullptr;
                        }
                    }
                    if ( bComplete ) return;
                    else if ( throwMe ) std::rethrow_exception(throwMe);
                    else throw exception_aborted();
                } else if ( waitFor )  {
                    abort.waitForEvent(*waitFor, -1);
                }
            }
		}
	private:
        std::shared_ptr<pfc::event> make_event() {
            if (!m_event) m_event = std::make_shared<pfc::event>();
            return m_event;
        }
		std::shared_ptr<pfc::event> m_event;
		pfc::mutex m_mutex;
		std::exception_ptr m_throwMe;
		bool m_started = false;
        bool m_complete = false;
	};
}
