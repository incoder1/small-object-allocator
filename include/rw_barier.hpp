#ifndef __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
#define __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__

#include <boost/config.hpp>

#ifdef __MINGW64__
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif // __MINGW64__

#if defined(BOOST_WINDOWS) && (_WIN32_WINNT >= 0x0600)
#		include "win/srwlock.hpp"
#elif defined(BOOST_POSIX_API)
#	include "posix/pthrwlock.hpp"
#else
#	include "shared_mutex_rwb.hpp"
#endif // BOOST_POSIX_API

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace sys {

class read_lock {
public:
	explicit read_lock(read_write_barier& barier):
		barier_(barier)
	{
		barier_.read_lock();
	}
	~read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.read_unlock();
	}
private:
	read_write_barier& barier_;
};

class write_lock
{
public:
	explicit write_lock(read_write_barier& barier):
		barier_(barier)
	{
		barier_.write_lock();
	}
	~write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.write_unlock();
	}
private:
	read_write_barier& barier_;
};

}} //  smallobject { namespace sys {

#endif // __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
