#ifndef __SYS_ALOC_ONCE__
#define __SYS_ALOC_ONCE__

#ifdef BOOST_WINDOWS

#include "win/heapallocator.hpp"

#else

namespace boost { namespace smallobject { namespace sys {

#ifdef BOOST_POSIX_API

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
#endif

#endif // BOOST_WINDOWS

#endif // __SYS_ALOC_ONCE__
