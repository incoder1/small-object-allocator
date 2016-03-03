#ifndef __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
#define __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__

#ifdef BOOST_WINDOWS
#	ifndef WINDOWS_NATIVE_READ_WRITE_BARIER
#		if _WIN32_WINNT >= 0x0600
#			define WINDOWS_NATIVE_READ_WRITE_BARIER
#			include <windows.h>
#		endif // WINDOWS_NATIVE_READ_WRITE_BARIER
#	endif // WINDOWS_NATIVE_READ_WRITE_BARIER
#endif // BOOST_WINDOWS

#ifdef BOOST_POSIX_API
#	ifndef PTHREAD_NATIVE_READ_WRITE_BARIER
#		define PTHREAD_NATIVE_READ_WRITE_BARIER
#		include <pthread.h>
#	endif // PTHREAD_NATIVE_READ_WRITE_BARIER
#endif // BOOST_POSIX_API

namespace boost { namespace smallobject { namespace sys {

#ifdef WINDOWS_NATIVE_READ_WRITE_BARIER
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
		::AcquireSRWLockExclusive(&barier_);
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
        ::ReleaseSRWLockExclusive(&barier_);
	}
private:
	::SRWLOCK barier_;
};

#elif defined(PTHREAD_NATIVE_READ_WRITE_BARIER)

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
		::pthread_rwlockattr_t attr;
		::pthread_rwlockattr_init(&attr);
		::pthread_rwlock_init(&barier_,&attr);
		::pthread_rwlockattr_destroy(&attr);
	}
	~read_write_barier() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_destroy(&barier_);
	}
	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_rdlock(&barier_);
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_unlock(&barier_);
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_wrlock(&barier_);
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::pthread_rwlock_unlock(&barier_);
	}
private:
	::pthread_rwlock_t barier_;
};

#else // boost based when no platform native rw barier

#include <boost/thread/shared_mutex.hpp>

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
#ifndef BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
	read_write_barier() = default;
	~read_write_barier() = default;
#else
	read_write_barier()
	{}
	~read_write_barier()
	{}
#endif // read_write_barier
	inline void read_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.lock_shared();
	}
	inline void read_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.unlock_shared();
	}
	inline void write_lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.lock_shared();
		barier_.unlock_and_lock_upgrade();
	}
	inline void write_unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		barier_.unlock_upgrade();
	}
private:
	boost::shared_mutex barier_;
};
#endif // NATIVE_READ_WRITE_BARIER

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

}}} // namespace boost { namespace smallobject { namespace sys

#endif // __SMALL_OBJECT_RW_BARIER_HPP_INCLUDED__
