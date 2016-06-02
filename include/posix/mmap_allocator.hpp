#ifndef __POSIX_MMAP_ALLOC_HPP_INCLUDED__
#define __POSIX_MMAP_ALLOC_HPP_INCLUDED__

#include <boost/throw_exception.hpp>

#include <unistd.h>
#include <sys/mman.h>

namespace smallobject { namespace sys {

/// Posix system memory allocator based on mmap
void* xmalloc(const std::size_t size)
{
	void *addr = NULL;
	const std::size_t block_size = size + sizeof(std::size_t);
	void *result = ::mmap(addr, block_size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
	if (result == MAP_FAILED || (NULL != addr && result != addr) ) {
        if(NULL != addr && result != addr) {
             ::munmap( result, size);
        }
		boost::throw_exception( std::bad_alloc() );
 	}
 	*(static_cast<std::size_t*>(result)) = block_size;
	return (result+sizeof(std::size_t));
}

/// Posix system memory deallocator based on munmap
BOOST_FORCEINLINE void xfree(void * const ptr)
{
	assert( ptr );
	std::size_t* block = static_cast<std::size_t*>(ptr) - 1;
    ::munmap(static_cast<void*>(block), *block);
}

}} /// namespace smallobject { namespace sys

#endif // __POSIX_MMAP_ALLOC_HPP_INCLUDED__
