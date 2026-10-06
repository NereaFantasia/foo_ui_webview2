#pragma once
#include <pfc/pool.h>
#include <functional>
#include "rethrow.h"
#include <pfc/timers.h>
#include <SDK/threadsLite.h>

#include <deque>
#include <mutex>
#include <optional>
#include <semaphore>

namespace ThreadUtils {

    template<typename elem_t>
    class waitQueueLite {
        std::deque<elem_t> m_deque;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    public:
        elem_t get() {
            std::unique_lock lock(m_mutex);
            m_cond.wait(lock, [this] { return !m_deque.empty(); });
            if (m_deque.empty()) return {};
            elem_t ret = std::move(m_deque.front());
            m_deque.pop_front();
            return ret;
        }
        std::optional< elem_t > get(double timeout) {
            std::unique_lock lock(m_mutex);
            if (!m_cond.wait_for(lock, std::chrono::duration<double>(timeout), [this] { return !m_deque.empty(); }))
                return std::nullopt;
            if (m_deque.empty()) return elem_t{};
            auto ret = std::move(m_deque.front());
            m_deque.pop_front();
            return std::move(ret);
        }
        template<typename arg_t>
        void put(arg_t&& arg) {
            std::lock_guard lock(m_mutex);
            m_deque.emplace_back(std::forward<arg_t>(arg));
            m_cond.notify_one();
        }
    };

    typedef std::function<bool(pfc::eventHandle_t, double) > waitFunc_t;

	// Serialize access to some resource to a single thread
	// Execute blocking/nonabortable methods in with proper abortability (detach on abort and move on)
	class cmdThread {
	public:
		typedef std::function<void () > func_t;
		typedef waitQueueLite<func_t> queue_t;
		typedef std::function<void (abort_callback&) > funcAbortable_t;
        
    private:
        std::function<void () > makeWorker() {
            auto q = m_queue;
            auto x = m_atExit;
            return [q, x] {
                for ( ;; ) {
                    auto f = q->get();
                    if (!f) break;
                    try { f(); } catch(...) {}
                }
                // No guard for atExit access, as nobody is supposed to be still able to call host object methods by the time we get here
                for( auto & f : *x) {
                    try { f(); } catch(...) {}
                }
            };
        };
        std::function<void () > makeWorker2( std::function<void()> updater, double interval) {
            PFC_ASSERT(interval > 0);
            auto q = m_queue;
            auto x = m_atExit;
            return [=] {
                pfc::lores_timer t; t.start();
                for ( ;; ) {
                    const double left = interval - t.query();
                    func_t work;
                    if ( left > 0 ) {
                        auto status = q->get(left);
                        if (status) {
                            work = std::move(*status);
                            if (!work) break;
                        }
                    }
                    if (work) {
                        try { work(); } catch (...) {}
                    } else {
                        updater();
                        t.start();
                    }
                }
                // No guard for atExit access, as nobody is supposed to be still able to call host object methods by the time we get here
                for (auto& f : *x) {
                    try { f(); } catch (...) {}
                }
            };
        };
        
    public:
        cmdThread(fb2k::thread::arg_t const& arg = fb2k::thread::argCurrentThread(), std::function<void()> updater = nullptr, double interval = 0) {
            std::function<void() > work;
            if (updater) work = makeWorker2(updater, interval);
            else work = makeWorker();
            m_thread.startHere(arg, work);
        }
        
		void atExit( func_t f ) {
			m_atExit->push_back(f);
		}
		~cmdThread() noexcept {
			m_queue->put(nullptr);
            m_thread.waitTillDone();
		}
		void shutdown(bool graceful) {
            if (!graceful) {
                auto evt = std::make_shared<std::binary_semaphore>(0);
                atExit( [evt] { evt->release();});
                m_queue->put(nullptr);
                evt->acquire();
            } else {
                m_queue->put(nullptr);
            }
        }
        void runSynchronously( func_t f ) {
            ThreadUtils::CRethrow rethrow;
            std::binary_semaphore evt { 0 };
            auto worker2 = [&] {
                rethrow.exec(f);
                evt.release();
            };
            add( worker2 );
            evt.acquire();
            rethrow.rethrow();
        }
		void runSynchronously( func_t f, abort_callback & abort ) {
            auto evt = m_eventPool.make();
            evt->set_state(false);
            auto rethrow = std::make_shared<ThreadUtils::CRethrow>();
            auto worker2 = [f, rethrow, evt] {
                rethrow->exec(f);
                evt->set_state( true );
            };

            add ( worker2 );
            
            abort.waitForEvent(*evt, -1);

            m_eventPool.put( std::move(evt) );

            rethrow->rethrow();
		}
		void runSynchronously2( funcAbortable_t f, abort_callback & abort ) {
			auto subAbort = m_abortPool.make();
			subAbort->reset();
			auto worker = [subAbort, f] {
				f(*subAbort);
			};

			try {
				runSynchronously( worker, abort );
			} catch(...) {
				subAbort->set(); throw;
			}

			m_abortPool.put( std::move( subAbort ) );
		}

		void add( func_t f ) { m_queue->put( f ); }
	private:
		pfc::objPool<pfc::event> m_eventPool;
		pfc::objPool<abort_callback_impl> m_abortPool;
		std::shared_ptr<queue_t> m_queue = std::make_shared<queue_t>();
		typedef std::vector<func_t> atExit_t;
		std::shared_ptr<atExit_t> m_atExit = std::make_shared< atExit_t >();
        fb2k::thread m_thread;
	};
}
