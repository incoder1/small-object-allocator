#ifndef _SMALLOBJECT_SHARED_MUTEX_RWB_HPP_INCLUDED__
#define _SMALLOBJECT_SHARED_MUTEX_RWB_HPP_INCLUDED__

#include <boost/throw_exception.hpp>
#include <boost/thread/shared_mutex.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace sys {

/// Boost thread shared mutex based slim reader/writter barrier implemenation
class read_write_barrier {
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
	read_write_barrier(const read_write_barrier&) = delete;
	read_write_barrier& operator=(const read_write_barrier&) = delete;
#else
private:
	read_write_barrier(const read_write_barrier&);
	read_write_barrier& operator=(const read_write_barrier&);
#endif // BOOST_NO_CXX11_DELETED_FUNCTIONS
public:

	read_write_barrier():
		barier_()
	{}

	~read_write_barrier()
	{}

	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW {
		barier_.lock_shared();
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW {
		barier_.unlock_shared();
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW {
		barier_.lock_upgrade();
		barier_.unlock_upgrade_and_lock();
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW {
		barier_.unlock();
	}
private:
	boost::shared_mutex barier_;
};

}} // namespace smallobject { namespace sys

#endif // _SMALLOBJECT_SHARED_MUTEX_RWB_HPP_INCLUDED__
