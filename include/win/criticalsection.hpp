#ifndef __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__
#define __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__

#include <boost/config.hpp>
#include <boost/thread/locks.hpp>
#include <boost/noncopyable.hpp>
#include <windows.h>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#ifndef _SOBJ_SPINCOUNT
#	define	_SOBJ_SPINCOUNT 4000
#endif

namespace smallobject { namespace sys {

/// !\brief MS Windows depended implementation of critical section
///  synchronization primitive
/// In most cases doing spinlock, and semaphore when spinning reached to _SOBJ_SPINCOUNT
class critical_section:private boost::noncopyable
{
	public:
		critical_section() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::InitializeCriticalSectionAndSpinCount(&cs_, _SOBJ_SPINCOUNT);
		}
		~critical_section() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::DeleteCriticalSection(&cs_);
		}
		BOOST_FORCEINLINE void lock() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::EnterCriticalSection(&cs_);
		}
		BOOST_FORCEINLINE bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW
		{
			return TRUE == ::TryEnterCriticalSection(&cs_);
		}
		BOOST_FORCEINLINE void unlock() BOOST_NOEXCEPT_OR_NOTHROW
		{
            ::LeaveCriticalSection(&cs_);
		}
	private:
		::CRITICAL_SECTION cs_;
};

}  // namespace smallobject

//typedef boost::unique_lock<sys::critical_section> unique_lock;

class unique_lock
{
public:
	unique_lock(sys::critical_section& cs) BOOST_NOEXCEPT_OR_NOTHROW:
		cs_(cs)
	{
		cs_.lock();
	}
	inline ~unique_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		cs_.unlock();
	}
private:
	sys::critical_section& cs_;
};

} //  { namespace sys

#endif // __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__
