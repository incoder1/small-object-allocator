#ifndef __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__
#define __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__

#if _WIN32_WINNT < 0x0600
#error "SWRLock can be used only on Windows Vista +"
#endif // _WIN32_WINNT

#include <boost/config.hpp>

#include <windows.h>

#ifdef __MINGW64__
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;
extern "C" {
  WINBASEAPI VOID WINAPI InitializeSRWLock (PSRWLOCK SRWLock);
  VOID WINAPI ReleaseSRWLockExclusive (PSRWLOCK SRWLock);
  VOID WINAPI ReleaseSRWLockShared (PSRWLOCK SRWLock);
  VOID WINAPI AcquireSRWLockExclusive (PSRWLOCK SRWLock);
  VOID WINAPI AcquireSRWLockShared (PSRWLOCK SRWLock);
  WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive (PSRWLOCK SRWLock);
  WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockShared (PSRWLOCK SRWLock);
}
#endif // __MINGW64__


namespace smallobject { namespace sys {

class read_write_barier
{
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
	read_write_barier(const read_write_barier&) = delete;
	read_write_barier& operator=(const read_write_barier&) = delete;
#else
private:
	read_write_barier(const read_write_barier&);
	read_write_barier& operator=(const read_write_barier&);
#endif // BOOST_NO_CXX11_DELETED_FUNCTIONS
public:
	read_write_barier() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::InitializeSRWLock(&barier_);
	}
	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::AcquireSRWLockShared(&barier_);
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::ReleaseSRWLockShared(&barier_);
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::AcquireSRWLockShared(&barier_);
		::ReleaseSRWLockShared(&barier_);
		::AcquireSRWLockExclusive(&barier_);
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
        ::ReleaseSRWLockExclusive(&barier_);
	}
private:
	::SRWLOCK barier_;
};

} } // namespace smallobject { namespace sys

#endif // __SMALLOBJECT_SRWLOCK_HPP_INCLUDED__
