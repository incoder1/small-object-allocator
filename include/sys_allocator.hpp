#ifndef __SYS_ALOC_ONCE__
#define __SYS_ALOC_ONCE__

#include <boost/config.hpp>
#include <boost/pool/pool_alloc.hpp>

#ifdef BOOST_WINDOWS
#	include "win/heapallocator.hpp"
#elif defined(BOOST_POSIX_API) && !defined(BOOST_WINDOWS)
#	include "posix/mmap_allocator.hpp"
#endif // defined

#include "critical_section.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace sys {

#if !defined(BOOST_WINDOWS) && !defined(BOOST_POSIX_API)
BOOST_FORCEINLINE void* xmalloc(const std::size_t size) {
	return ::malloc(size);
}
BOOST_FORCEINLINE void xfree(void * const ptr,const std::size_t size = 0) {
    ::free(ptr);
}
#endif // !defined(BOOST_WINDOWS) && !defined(BOOST_POSIX_API)

#ifdef BOOST_POSIX_API
typedef boost::default_user_allocator_malloc_free user_allocator;
#endif // BOOST_WINDOWS

#ifndef BOOST_POSIX_API
struct user_allocator
{
  typedef std::size_t size_type; //!< An unsigned integral type that can represent the size of the largest object to be allocated.
  typedef std::ptrdiff_t difference_type; //!< A signed integral type that can represent the difference of any two pointers.

  static char * malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const size_type bytes)
  { //! Attempts to allocate n bytes from the system. Returns 0 if out-of-memory
    return static_cast<char*>(xmalloc(bytes));
  }
  static void free BOOST_PREVENT_MACRO_SUBSTITUTION(char * const block)
  { //! Attempts to de-allocate block.
    //! \pre Block must have been previously returned from a call to UserAllocator::malloc.
	xfree(block, 0);
  }
};
#endif // !BOOST_POSIX_API

#ifndef __SM_INTERNAL_POOL_NEXT_SIZE
#	define __SM_SYS_POOL_NEXT_SIZE UCHAR_MAX
#	define __SM_SYS_POOL_MAX_SIZE __SM_SYS_POOL_NEXT_SIZE*2
#endif // __SM_INTERNAL_POOL_NEXT_SIZE

template<typename _DataType>
class allocator:public
	boost::fast_pool_allocator<
		_DataType,
		smallobject::sys::user_allocator,
		smallobject::sys::critical_section,
		__SM_SYS_POOL_NEXT_SIZE,
		__SM_SYS_POOL_MAX_SIZE>
{
private:
	typedef	boost::fast_pool_allocator<
		_DataType,
		smallobject::sys::user_allocator,
		smallobject::sys::critical_section,
		__SM_SYS_POOL_NEXT_SIZE,
		__SM_SYS_POOL_MAX_SIZE> base_type;
public:
	allocator():
		base_type()
	{}
};

}} /// namespace smallobject { namespace sys

#endif // __SYS_ALOC_ONCE__
