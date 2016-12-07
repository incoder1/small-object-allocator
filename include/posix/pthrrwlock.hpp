#ifndef __SMALL_OBJECT_PTHRRWLOCK_HPP_INCLUDED__
#define __SMALL_OBJECT_PTHRRWLOCK_HPP_INCLUDED__

#include <boost/config.hpp>
#include <boost/thread/locks.hpp>
#include <pthread.h>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace sys {

class read_write_barrier
{
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
	read_write_barrier(const read_write_barrier&) = delete;
	read_write_barrier& operator=(const read_write_barrier&) = delete;
#else
private:
	read_write_barier(const read_write_barier&);
	read_write_barier& operator=(const read_write_barier&);
#endif // BOOST_NO_CXX11_DELETED_FUNCTIONS
public:
	read_write_barrier() BOOST_NOEXCEPT_OR_NOTHROW:
        barrier_()
	{
		::pthread_rwlockattr_t attr;
		::pthread_rwlockattr_init(&attr);
		::pthread_rwlock_init(&barrier_,&attr);
		::pthread_rwlockattr_destroy(&attr);
	}
	~read_write_barrier() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_destroy(&barrier_);
	}
	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_rdlock(&barrier_);
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_unlock(&barrier_);
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_wrlock(&barrier_);
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_unlock(&barrier_);
	}
private:
	::pthread_rwlock_t barrier_;
};

}} // namespace smallobject { namespace sys

#endif // __SMALL_OBJECT_PTHRRWLOCK_HPP_INCLUDED__
