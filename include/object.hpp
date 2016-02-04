#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include <cstddef>
#include <new>
#include <vector>

#include "nb_forward_list.hpp"

#include <boost/intrusive_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#ifdef BOOST_WINDOWS_API
#	include <windows.h>
#elif BOOST_POSIX_API
#	include <unistd.h>
#endif // BOOST_WINDOWS_API

#ifndef BOOST_THROWS
#	ifdef BOOST_NO_CXX11_NOEXCEPT
#		define BOOST_THROWS(_EXC) throw(_EXC)
#	else
#		define BOOST_THROWS(_EXC)
#	endif  // BOOST_NO_CXX11_NOEXCEPT
#endif // BOOST_THROWS

namespace boost {

namespace smallobject {

class BOOST_SYMBOL_VISIBLE object
{
protected:
	object() BOOST_NOEXCEPT_OR_NOTHROW;
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
public:
      object& operator=( const object& ) = delete;
	  BOOST_CONSTEXPR object(object&& rhs) = delete;
	  object& operator=(object&& rhs) = delete;
#else
private:  // emphasize the following members are private
      object( const object& );
      object& operator=( const object& );
#endif // BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
public:
	virtual ~object() BOOST_NOEXCEPT_OR_NOTHROW = 0;
	// redefine new and delete operations for small object optimized memory allocation
	void* operator new(std::size_t s) BOOST_THROWS(std::bad_alloc);
	void operator delete(void *ptr,std::size_t s) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	// ass support for boost::intrusive_ptr
	boost::atomics::atomic_size_t ref_count_;
	friend void intrusive_ptr_add_ref(object* obj);
	friend void intrusive_ptr_release(object* const obj);
};

} // namespace smallobject

using smallobject::object;

#ifndef BOOST_DECLARE_OBJECT_PTR_T
#	define BOOST_DECLARE_OBJECT_PTR_T(TYPE) typedef boost::intrusive_ptr<TYPE> s_##TYPE
#endif // DECLARE_SPTR_T

} // namespace boost

#endif // __LIBGC_OBJECT_HPP_INCLUDED__
