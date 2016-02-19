#   ifndef __SYS_ALOC_ONCE__
#   define __SYS_ALOC_ONCE__

#ifdef BOOST_WINDOWS
#   include "win/heapallocator.hpp"
#else

#include <boost/config.hpp>
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

#endif // platform switch

#   endif // __SYS_ALOC_ONCE__
