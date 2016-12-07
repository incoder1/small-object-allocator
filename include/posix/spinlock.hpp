#ifndef __SMALLOBJECT_UNIX_SPINLOCK_HPP_INCLUDED__
#define __SMALLOBJECT_UNIX_SPINLOCK_HPP_INCLUDED__

#include <boost/noncopyable.hpp>
#include <boost/thread/locks.hpp>
#include <pthread.h>

namespace smallobject { namespace sys {

/// !\brief pthread spinlock based critical section
/// synchronization primitive
class critical_section:private boost::noncopyable
{
	public:
		critical_section()
		{
			::pthread_spin_init(&sl_, PTHREAD_PROCESS_PRIVATE);
		}
		~critical_section() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::pthread_spin_destroy(&sl_);
		}
		BOOST_FORCEINLINE void lock() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::pthread_spin_lock(&sl_);
		}
		BOOST_FORCEINLINE bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW
		{
			return ::pthread_spin_trylock(&sl_);
		}
		BOOST_FORCEINLINE void unlock() BOOST_NOEXCEPT_OR_NOTHROW
		{
			::pthread_spin_unlock(&sl_);
		}
	private:
		::pthread_spinlock_t sl_;
};

} //  namespace sys

//typedef boost::unique_lock<sys::critical_section> unique_lock;

class unique_lock final: private boost::noncopyable {
public:
    unique_lock(sys::critical_section& cs) BOOST_NOEXCEPT_OR_NOTHROW:
        cs_(cs)
    {
        cs.lock();
    }
    ~unique_lock() BOOST_NOEXCEPT_OR_NOTHROW
    {
        cs_.unlock();
    }
private:
    sys::critical_section& cs_;
};


} // namespace smallobject

#endif // __SMALLOBJECT_UNIX_SPINLOCK_HPP_INCLUDED__
