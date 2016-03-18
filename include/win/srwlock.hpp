#ifndef __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__
#define __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#if _WIN32_WINNT < 0x0600
#error "SWRLock can be used only on Windows Vista +"
#endif // _WIN32_WINNT

#ifndef DECLSPEC_IMPORT
#	if (defined (__i386__) || defined (__ia64__) || defined (__x86_64__) || defined (__arm__)) && !defined (__WIDL__)
#		define DECLSPEC_IMPORT __declspec (dllimport)
#	else
#		define DECLSPEC_IMPORT
#	endif
#endif // DECLSPEC_IMPORT

#ifndef WINBASEAPI
#	ifndef _KERNEL32_
#		define WINBASEAPI DECLSPEC_IMPORT
#	else
#		define WINBASEAPI
#	endif
#endif // WINBASEAPI

#ifndef WINAPI
#	if defined(_ARM_)
#		define WINAPI
#	else
#		define WINAPI __stdcall
#	endif
#endif // WINAPI


namespace smallobject { namespace sys {

namespace win32 {
	typedef struct _RTL_SRWLOCK { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
	extern "C" {
		WINBASEAPI VOID WINAPI InitializeSRWLock (PSRWLOCK SRWLock);
		VOID WINAPI ReleaseSRWLockExclusive (PSRWLOCK SRWLock);
		VOID WINAPI ReleaseSRWLockShared (PSRWLOCK SRWLock);
		VOID WINAPI AcquireSRWLockExclusive (PSRWLOCK SRWLock);
		VOID WINAPI AcquireSRWLockShared (PSRWLOCK SRWLock);
		WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive (PSRWLOCK SRWLock);
		WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockShared (PSRWLOCK SRWLock);
	} // extern "C"
}


/// Windows Vista+ SWR lock slim reader/writer barrier implementation
class read_write_barrier
{
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
	read_write_barrier(const read_write_barrier&) = delete;
	read_write_barrier& operator=(const read_write_barrier&) = delete;
#else
private:
	read_write_barrier(const read_write_barrier&);
	read_write_barrier& operator=(const read_write_barrier&);
#endif // BOOST_NO_CXX11_DELETED_FUNCTIONS
public:
	read_write_barrier() BOOST_NOEXCEPT_OR_NOTHROW
	{
		win32::InitializeSRWLock(&barier_);
	}
	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		win32::AcquireSRWLockShared(&barier_);
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		win32::ReleaseSRWLockShared(&barier_);
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		win32::AcquireSRWLockExclusive(&barier_);
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
        win32::ReleaseSRWLockExclusive(&barier_);
	}
private:
	win32::SRWLOCK barier_;
};

} } // namespace smallobject { namespace sys

#endif // __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__
