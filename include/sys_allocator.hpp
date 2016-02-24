#ifndef __SYS_ALOC_ONCE__
#define __SYS_ALOC_ONCE__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#if defined(BOOST_WINDOWS_API)
#	include "win/heapallocator.hpp"
#elif defined(BOOST_POSIX_API)
#	include "posix/aligned_malloc.hpp"
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

#include <limits>

namespace boost { namespace smallobject { namespace sys {

template<typename T>
class allocator {
public :
    //    typedefs
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

public :
    //    convert an allocator<T> to allocator<U>
    template<typename U>
    struct rebind {
        typedef allocator<U> other;
    };

public :
    inline explicit allocator()
    {}

    inline ~allocator()
    {}

    inline explicit allocator(allocator const&)
    {}

    template<typename U>
    inline explicit allocator(allocator<U> const&)
    {}

    //    address
    inline pointer address(reference r) {
    	 return &r;
	}

    inline const_pointer address(const_reference r) {
    	return &r;
	}

    //    memory allocation
    inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0)
    {
    	void * ptr = boost::smallobject::sys::xmalloc( cnt * sizeof(T) );
		return static_cast<pointer>( ptr );
    }

    inline void deallocate(pointer p, size_type cnt)
    {
        boost::smallobject::sys::xfree( p, cnt);
    }

    //    size
    inline size_type max_size() const
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
	}

    //    construction/destruction
    inline void construct(pointer p, const T& t)
    {
		new(p) T(t);
	}

    inline void destroy(pointer p)
    {
    	p->~T();
	}

    inline bool operator==(allocator const&)
    {
    	return true;
	}

    inline bool operator!=(allocator const& a)
    {
    	return !operator==(a);
	}
};

}}} /// namespace boost { namespace smallobject { namespace sys

#endif // __SYS_ALOC_ONCE__
