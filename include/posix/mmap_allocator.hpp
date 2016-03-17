#ifndef __POSIX_MMAP_ALLOC_HPP_INCLUDED__
#define __POSIX_MMAP_ALLOC_HPP_INCLUDED__

#include <boost/throw_exception.hpp>

#include <unistd.h>
#include <sys/mman.h>

namespace smallobject { namespace sys {

/// Posix system memory allocator based on mmap
inline void* xmalloc(std::size_t size)
{
	void *addr = NULL;
	void *result = ::mmap(addr, size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
	if (result == MAP_FAILED || (NULL != addr && result != addr) ) {
        if(NULL != addr && result != addr)
        {
             ::munmap( result, size);
        }
		boost::throw_exception( std::bad_alloc() );
 	}
 	::mlock(result, size);
	return (result);
}

/// Posix system memory deallocator based on munmap
BOOST_FORCEINLINE void xfree(void * const ptr, const std::size_t size)
{
	assert( ptr );
	::munlock(ptr,size);
    ::munmap( ptr, size);
}

}} /// namespace smallobject { namespace sys

#endif // __POSIX_MMAP_ALLOC_HPP_INCLUDED__
