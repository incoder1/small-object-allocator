#ifndef __SYS_ALOC_ONCE__
#define __SYS_ALOC_ONCE__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#include <boost/pool/pool_alloc.hpp>
#include "critical_section.hpp"

#if defined(_WIN32) || defined(_WIN64)
#	include "win/heapallocator.hpp"
#elif defined(BOOST_POSIX_API) && !defined(_WIN32) && !defined(_WIN64)
#	include "posix/mmap_allocator.hpp"
#else

namespace boost { namespace smallobject { namespace sys {

BOOST_FORCEINLINE void* xmalloc(std::size_t size) {
	return ::malloc(size);
}

BOOST_FORCEINLINE void xfree(void * const ptr) {
    ::free(ptr);
}

}}} // namespace boost { namespace smallobject { namespace sys

#endif

namespace boost { namespace smallobject { namespace sys {

struct user_allocator_xmalloc
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
	xfree(block);
  }
};

#define __BSM_INTERNAL_POOL_NEXT_SIZE 256
#define __BSM_INTERNAL_POOL_MAX_SIZE 512

template<typename _DataType>
class allocator:public
	boost::fast_pool_allocator<
		_DataType,
		user_allocator_xmalloc,
		critical_section,
		__BSM_INTERNAL_POOL_NEXT_SIZE,
		__BSM_INTERNAL_POOL_MAX_SIZE>
{
private:
	typedef	boost::fast_pool_allocator<
		_DataType,
		user_allocator_xmalloc,
		critical_section,
		__BSM_INTERNAL_POOL_NEXT_SIZE,
		__BSM_INTERNAL_POOL_MAX_SIZE> base_type;
public:
	allocator():
		base_type()
	{}
};

}}} /// namespace boost { namespace smallobject { namespace sys

#endif // __SYS_ALOC_ONCE__
