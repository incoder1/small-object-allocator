#ifndef __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__
#define __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#include "spinlock.hpp"
#include <boost/atomic.hpp>
#include <boost/noncopyable.hpp>

#include <windows.h>

namespace boost {

namespace smallobject {

namespace sys {

namespace detail {

class heap_allocator:private boost::noncopyable
{
private:
	heap_allocator():
		hHeap_(NULL)
	{
		hHeap_ = ::HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
	}
	static void release() BOOST_NOEXCEPT_OR_NOTHROW {
		delete _instance.load(memory_order::memory_order_consume);
		_instance.store(NULL, memory_order::memory_order_release);
	}
public:
	static const heap_allocator* instance() {
		heap_allocator* tmp = _instance.load(memory_order::memory_order_consume);
		if (!tmp) {
			boost::unique_lock<smallobject::spin_lock> lock(_mtx);
			if (!(tmp = _instance.load(memory_order::memory_order_consume))) {
				tmp = new heap_allocator();
				_instance.store(tmp, memory_order::memory_order_release);
				std::atexit(&heap_allocator::release);
			}
		}
		return tmp;
	}
	~heap_allocator() BOOST_NOEXCEPT_OR_NOTHROW
	{
		::HeapDestroy(hHeap_);
	}
	BOOST_FORCEINLINE void* allocate(std::size_t bytes) const {
		return ::HeapAlloc(hHeap_, 0, bytes);
	}
	BOOST_FORCEINLINE void release(void * const block) const {
		::HeapFree(hHeap_, 0, block);
	}
private:
	::HANDLE hHeap_;
	static smallobject::spin_lock _mtx;
	static boost::atomic<heap_allocator*> _instance;
};

} // namesapase detail

BOOST_FORCEINLINE void* xmalloc(std::size_t size)
{
	return detail::heap_allocator::instance()->allocate(size);
}

BOOST_FORCEINLINE void xfree(void * const ptr,const std::size_t size = 0)
{
	detail::heap_allocator::instance()->release(ptr);
}

} // namespace sys

}  // namespace smallobject

} // namespace boost

#endif // __SMALL_OBJECT_WIN_HEAP_ALLOCATOR_HPP_INCLUDED__
