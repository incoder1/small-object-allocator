#ifndef __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
#define __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__

#include <boost/config.hpp>

// Ming 64 attemps to use Windows XP by default
// Windows XP is no longer supported by Microsoft
#if defined(__MINGW64__) && (__GNUC__ >= 4)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif // __MINGW64__

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)
// Windows Vista+ can use SWR locks
#		include "win/srwlock.hpp"
#elif defined(BOOST_POSIX_API)
// Unix can use pthread bases read/write lock
#	include "posix/pthrwlock.hpp"
#else
// On legacy or unknown platforms shared mutex is default
// working slowly then native API in most cases
#	include "shared_mutex_rwb.hpp"
#endif // BOOST_POSIX_API

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace sys {

/// \brief Slim reader/writer shared lock
class read_lock {
public:
	explicit read_lock(read_write_barrier& barier):
		barier_(barier)
	{
		barier_.read_lock();
	}
	~read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.read_unlock();
	}
private:
	read_write_barrier& barier_;
};

/// \brief Slim reader/writer exclusive lock
class write_lock
{
public:
	explicit write_lock(read_write_barrier& barier):
		barier_(barier)
	{
		barier_.write_lock();
	}
	~write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.write_unlock();
	}
private:
	read_write_barrier& barier_;
};

}} //  smallobject { namespace sys {

#endif // __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
