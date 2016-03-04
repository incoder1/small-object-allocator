#include "object_allocator.hpp"

namespace smallobject { namespace detail {

static inline std::size_t align_up(const std::size_t alignment,const std::size_t size) BOOST_NOEXCEPT
{
	return ( size + (alignment - 1) ) & ~(alignment - 1);
}

// 64 for 32 bit and 128 for 64 bit instructions
static const std::size_t MAX_SIZE = sizeof(std::size_t) * 16;
// object_allocator
static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(basic_object) ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT; // count of small object pools = 14


const object_allocator* object_allocator::instance()
{
	object_allocator *tmp = _instance.load(boost::memory_order_relaxed);
	if (!tmp) {
		unique_lock lock(_smtx);
		tmp = _instance.load(boost::memory_order_relaxed);
		if (!tmp) {
			tmp = new object_allocator();
			_instance.store(tmp, boost::memory_order_release);
			boost::atomic_thread_fence(boost::memory_order_acquire);
			std::atexit(&object_allocator::release);
		}
	}
	return tmp;
}

object_allocator::object_allocator():
	pools_( NULL )
{
	pool_t* p = static_cast<pool_t*>(sys::xmalloc(POOLS_COUNT * sizeof(pool_t) ) );
	pools_= p;
	std::size_t pool_size = MIN_SIZE;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ ) {
		p = new (p) pool_t();
		pool_size += sizeof(std::size_t);
		++p;
	}
}

object_allocator::~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	pool_t* p = pools_;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
	{
		p->~pool_t();
		++p;
	}
	sys::xfree(pools_, sizeof(pool_t) * POOLS_COUNT );
}

BOOST_FORCEINLINE void* object_allocator::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) const
{
	pool_t* p = get(size);
	assert(p);
	return p->malloc(size);
}

BOOST_FORCEINLINE void object_allocator::free BOOST_PREVENT_MACRO_SUBSTITUTION(void* const ptr, const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
{
	pool_t* p = get(size);
	assert(p);
	p->free(ptr);
}

BOOST_FORCEINLINE object_allocator::pool_t* object_allocator::get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
{
	std::size_t index = ( align_up(sizeof(std::size_t),size) / sizeof(std::size_t) ) - SHIFT;
	return pools_ + index;
}

void object_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	delete _instance.load(boost::memory_order_relaxed);
}

sys::critical_section object_allocator::_smtx;
boost::atomic<object_allocator*> object_allocator::_instance(NULL);

}} //  namespace smallobject { namespace detail
