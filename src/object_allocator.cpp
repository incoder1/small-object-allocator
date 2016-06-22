#include "object_allocator.hpp"

namespace smallobject { namespace detail {

// object_allocator
BOOST_CONSTEXPR_OR_CONST std::size_t object_allocator::MAX_SIZE = sizeof(std::size_t) * 16;
// 8 bytes for 32 bit and 16 bytes for 64 bit
BOOST_CONSTEXPR_OR_CONST std::size_t object_allocator::MIN_SIZE = align_up( sizeof(std::size_t), sizeof(std::size_t)*2 );
// 2 since object size not changed
BOOST_CONSTEXPR_OR_CONST std::size_t object_allocator::SHIFT = MIN_SIZE / sizeof(std::size_t);
// 14 pools
BOOST_CONSTEXPR_OR_CONST std::size_t object_allocator::POOLS_COUNT = ( ( object_allocator::MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT; // count of small object pools = 14

object_allocator* object_allocator::instance()
{
	object_allocator *tmp = _instance.load(boost::memory_order_consume);
	if (!tmp) {
		unique_lock lock(_smtx);
		tmp = _instance.load(boost::memory_order_consume);
		if (!tmp) {
			tmp = new object_allocator();
			_instance.store(tmp, boost::memory_order_release);
			std::atexit(&object_allocator::release);
		}
	}
	return tmp;
}

object_allocator::object_allocator():
	pools_( NULL )
{
	pool* p = static_cast<pool*>(sys::xmalloc(POOLS_COUNT * sizeof(pool) ) );
	pools_= p;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ ) {
		p = new (p) pool();
		++p;
	}
}

object_allocator::~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	pool* p = pools_;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
	{
		p->~pool();
		++p;
	}
	sys::xfree(pools_);
}

void object_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	object_allocator* instance = _instance.load(boost::memory_order_relaxed);
	delete instance;
	_instance.store(NULL);
}

sys::critical_section object_allocator::_smtx;
boost::atomic<object_allocator*> object_allocator::_instance(NULL);

}} //  namespace smallobject { namespace detail
