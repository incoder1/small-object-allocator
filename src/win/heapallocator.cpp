#include "heapallocator.hpp"

namespace smallobject { namespace sys {

sys::critical_section heap_allocator::_mtx;
boost::atomic<heap_allocator*> heap_allocator::_instance(NULL);

heap_allocator* heap_allocator::instance() {
	heap_allocator* tmp = _instance.load(boost::memory_order_consume);
	if (!tmp) {
		unique_lock lock(_mtx);
		tmp = _instance.load(boost::memory_order_consume);
		if (NULL == tmp) {
			::HANDLE heap = ::HeapCreate( 0, 0, 0);
			void* ptr = ::HeapAlloc(heap, HEAP_NO_SERIALIZE, sizeof(heap_allocator) ) ;
			tmp = new (ptr) heap_allocator(heap);
			_instance.store(tmp, boost::memory_order_release);
			std::atexit(&heap_allocator::release);
		}
	}
	return tmp;
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
