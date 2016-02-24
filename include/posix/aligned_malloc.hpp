#ifndef __POSIX_ALIGNED_MALLOC_HPP_INCLUDED__
#define __POSIX_ALIGNED_MALLOC_HPP_INCLUDED__

#include <stdlib.h>

namespace boost { namespace smallobject { namespace sys {

BOOST_FORCEINLINE void* xmalloc(std::size_t size)
{
    void *result = NULL;
	::posix_memalign(&result, ::sysconf(_SC_PAGESIZE), size);
	return result;
}

BOOST_FORCEINLINE void xfree(void * const ptr)
{
    ::free(ptr);
}

}}} /// namespace boost { namespace smallobject { namespace sys

#endif // __POSIX_ALIGNED_MALLOC_HPP_INCLUDED__
