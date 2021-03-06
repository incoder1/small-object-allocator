#include "heapallocator.hpp"

namespace smallobject { namespace sys {

sys::critical_section heap_allocator::_mtx;
boost::atomic<heap_allocator*> heap_allocator::_instance(NULL);

heap_allocator* heap_allocator::instance() BOOST_NOEXCEPT_OR_NOTHROW {
	heap_allocator* result = _instance.load(boost::memory_order_consume);
	if (!result) {
		unique_lock lock(_mtx);
		result = _instance.load(boost::memory_order_consume);
		if (NULL == result) {
			::HANDLE heap = ::HeapCreate( 0, 0, 0);
			if(INVALID_HANDLE_VALUE == heap)
				std::terminate();
			void* ptr = ::HeapAlloc(heap, HEAP_NO_SERIALIZE, sizeof(heap_allocator) ) ;
			if(NULL == ptr)
				std::terminate();
			result = new (ptr) heap_allocator(heap);
			_instance.store(result, boost::memory_order_release);
			std::atexit(&heap_allocator::release);
		}
	}
	return result;
}

void heap_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	heap_allocator *al = _instance.load(boost::memory_order_acquire);
	::HANDLE heap = al->hHeap_;
	al->~heap_allocator();
	::HeapFree(heap, 0, al);
	::HeapDestroy(heap);
	_instance.store(NULL, boost::memory_order_release);
}

}} ///  namespace smallobject { namespace sys
