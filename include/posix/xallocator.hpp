#ifndef __POSIX_MMAP_ALLOC_HPP_INCLUDED__
#define __POSIX_MMAP_ALLOC_HPP_INCLUDED__

#include <boost/throw_exception.hpp>

#include <stdlib.h>
#include <unistd.h>

namespace smallobject { namespace sys {

static BOOST_CONSTEXPR_OR_CONST std::size_t _x_align = sizeof(std::size_t) * 2;

/// Posix system memory allocator
BOOST_FORCEINLINE  void* xmalloc(const std::size_t size)
{
     void* result = NULL;
     ::posix_memalign(&result, _x_align, size);
     return result;
}

/// Posix system memory deallocator
BOOST_FORCEINLINE void xfree(void * const ptr)
{
	assert( ptr );
    return ::free(ptr);
}

}} /// namespace smallobject { namespace sys

#endif // __POSIX_MMAP_ALLOC_HPP_INCLUDED__
