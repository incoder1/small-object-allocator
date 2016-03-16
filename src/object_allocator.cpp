#include "object_allocator.hpp"

namespace smallobject { namespace detail {

static inline std::size_t align_up(const std::size_t alignment,const std::size_t size) BOOST_NOEXCEPT
{
	return ( size + (alignment - 1) ) & ~(alignment - 1);
}


// object_allocator
static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(std::size_t)*2 ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( object_allocator::MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT; // count of small object pools = 14


const object_allocator* object_allocator::instance()
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
	sys::xfree(pools_, sizeof(pool) * POOLS_COUNT );
}

pool* object_allocator::get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
{
	std::size_t index = ( align_up(sizeof(std::size_t),size) / sizeof(std::size_t) ) - SHIFT;
	return pools_ + index;
}

void object_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	object_allocator* instance = _instance.load(boost::memory_order_relaxed);
	delete instance;
	_instance.store(NULL);
}

sys::critical_section object_allocator::_smtx;
boost::atomic<object_allocator*> object_allocator::_instance(NULL);

}} //  namespace smallobject { namespace detail
