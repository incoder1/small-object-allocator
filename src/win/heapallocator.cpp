#include "heapallocator.hpp"

namespace smallobject { namespace sys {

sys::critical_section heap_allocator::_mtx;
boost::atomic<heap_allocator*> heap_allocator::_instance(NULL);

const heap_allocator* heap_allocator::instance() {
	heap_allocator* tmp = _instance.load(boost::memory_order_relaxed);
	if (!tmp) {
		smallobject::unique_lock lock(_smtx);
		tmp = _instance.load(boost::memory_order_acquire);
		if (NULL == tmp) {
			::HANDLE heap = ::HeapCreate( 0, 0, 0);
			tmp = static_cast<heap_allocator*>( ::HeapAlloc(heap, 0, sizeof(heap_allocator) ) );
			tmp = new ( static_cast<void*>(tmp) ) heap_allocator(heap);
			_instance.store(tmp, memory_order_release);
			std::atexit(&heap_allocator::release);
		}
	}
	return tmp;
}

void heap_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	heap_allocator *al = _instance.load(boost::memory_order_relaxed);
	::HANDLE heap = al->hHeap_;
	al->~heap_allocator();
	::HeapFree(heap, 0, al);
	::HeapDestroy(heap);
}

}} ///  namespace smallobject { namespace sys
