#ifndef __POSIX_MMAP_ALLOC_HPP_INCLUDED__
#define __POSIX_MMAP_ALLOC_HPP_INCLUDED__

#include <sys/mman.h>

namespace boost { namespace smallobject { namespace sys {

BOOST_FORCEINLINE void* xmalloc(std::size_t size)
{
	void *addr = NULL;
	void *result = ::mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (result == MAP_FAILED) {
		result = NULL;
 	} else if (addr != NULL && ret != result) {
		// We succeeded in mapping memory, but not in the right place.
		xfree(result, size);
		result = NULL;
	}
	if(result == NULL || (addr == NULL && result != addr) || (addr != NULL && result == addr)) {
		boost::throw_exception( std::bad_alloc() );
	}
	return (result);
}

BOOST_FORCEINLINE void xfree(void * const ptr, const std::size_t size)
{
	assert( ptr );
	return ::munmap( ptr, size);
}

}}} /// namespace boost { namespace smallobject { namespace sys

#endif // __POSIX_MMAP_ALLOC_HPP_INCLUDED__
