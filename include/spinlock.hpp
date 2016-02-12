#ifndef __SMALL_OBJECT_SPINLOCK_HPP_INCLUDED__
#define __SMALL_OBJECT_SPINLOCK_HPP_INCLUDED__

#include <boost/atomic.hpp>
#include <boost/thread/thread_only.hpp>
#include <boost/noncopyable.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#ifndef _CRT_SPINCOUNT
#	define	_CRT_SPINCOUNT 4000
#endif

namespace boost {

namespace smallobject {

class spin_lock:private boost::noncopyable {
private:
	enum _state {LOCKED, UNLOCKED};
public:
	spin_lock() BOOST_NOEXCEPT_OR_NOTHROW:
		state_(UNLOCKED)
	{}
	inline void lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		std::size_t spin_count = 0;
		while( !state_.exchange(LOCKED, boost::memory_order_acquire) )
		{
            if(++spin_count == _CRT_SPINCOUNT) {
                boost::thread::yield();
                spin_count = 0;
            }
		}
	}
	BOOST_FORCEINLINE bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW {
		return state_.exchange(LOCKED, boost::memory_order_acquire);
	}
	BOOST_FORCEINLINE void unlock() BOOST_NOEXCEPT_OR_NOTHROW {
		state_.store(UNLOCKED, boost::memory_order_release);
	}
private:
	boost::atomic<_state> state_;
};

} // smallobject

} // boost

#endif // __SMALL_OBJECT_SPINLOCK_HPP_INCLUDED__
