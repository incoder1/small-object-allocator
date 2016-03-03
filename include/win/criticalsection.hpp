#ifndef __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__
#define __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__

#ifndef _SOBJ_SPINCOUNT
#	define	_SOBJ_SPINCOUNT 4000
#endif

#include <boost/noncopyable.hpp>
#include <windows.h>

namespace boost { namespace smallobject { namespace sys {

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

}}} /// namespace boost { namespace smallobject { namespace sys

#endif // __SMALL_OBJECT_WIN_CRITICALSECTION_HPP_INCLUDED__
