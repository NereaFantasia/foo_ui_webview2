#pragma once
#include <memory>
#include <exception>

namespace fb2k {
	static auto inMainThreadReturn(auto fn, abort_callback& a) {
		using ret_t = decltype(fn());
		auto ret = std::make_shared< std::pair<ret_t,std::exception_ptr> >();
		fb2k::inMainThreadSynchronous([fn, ret] {
			try {
				ret->first = fn();
			} catch (...) {
				ret->second = std::current_exception();
			}
		}, a);
		if (ret->second) std::rethrow_exception(ret->second);
		return std::move(ret->first);
	}
}