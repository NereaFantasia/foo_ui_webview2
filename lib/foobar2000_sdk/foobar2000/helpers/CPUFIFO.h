#pragma once
#include <list>
#include <functional>
#include <SDK/threadsLite.h>

namespace fb2k {
	// Helper class meant for static use to guarantee that multiple tasks sent to shared CPU pool are executed serially not concurrently
	class CPUFIFO {
	public:
		typedef std::function<void()> work_t;

		void add(work_t f) {
			PFC_INSYNC(workerSync);
			workQueue.push_back(std::move(f));
			if (!workerActive) {
				fb2k::inCpuWorkerThread([this] { this->workerProc(); });
				workerActive = true;
			}
		}
	private:
		void workerProc() noexcept {
			for (;;) {
				work_t work;
				{
					PFC_INSYNC(workerSync);
					PFC_ASSERT(workerActive);
					if (workQueue.empty()) { workerActive = false; return; }
					work = std::move(workQueue.front());
					workQueue.pop_front();
				}
				try { work(); } catch (...) { PFC_ASSERT(!"???"); }
			}
		}

		volatile bool workerActive = false;
		pfc::mutex workerSync;
		std::list<work_t> workQueue;
	};
}
