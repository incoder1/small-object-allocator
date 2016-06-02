#ifndef __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__
#define __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__

#include <boost/config.hpp>
#include <boost/atomic.hpp>

#include "criticalsection.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE


namespace smallobject { namespace sys {

/// \brief Windows based private application heap
/// memory allocator
class heap_allocator:private boost::noncopyable
{
private:
	explicit heap_allocator(::HANDLE heap):
		hHeap_(heap)
	{}
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
public:
	static heap_allocator* instance();

	BOOST_FORCEINLINE void* allocate(std::size_t bytes) {
		return ::HeapAlloc(hHeap_, 0, bytes);
	}

	BOOST_FORCEINLINE void release(void * const block) {
		::HeapFree(hHeap_, 0, block);
	}
private:
	::HANDLE hHeap_;
	static sys::critical_section _mtx;
	static boost::atomic<heap_allocator*> _instance;
};


BOOST_FORCEINLINE void* xmalloc(std::size_t size)
{
	return heap_allocator::instance()->allocate(size);
}

BOOST_FORCEINLINE void xfree(void * const ptr)
{
	heap_allocator::instance()->release(ptr);
}

} } // namespace smallobject { namespace sys

#endif // __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__
