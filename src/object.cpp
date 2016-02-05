#include "object.hpp"

namespace boost {

namespace smallobject {

const std::size_t MAX_SIZE = sizeof(std::size_t) * 16;

namespace detail {

// page
const uint8_t page::MAX_BLOCKS = static_cast<uint8_t>(UCHAR_MAX);

page::page(const uint8_t block_size):
	position_(0),
	free_blocks_(MAX_BLOCKS),
	begin_( new uint8_t[block_size*MAX_BLOCKS] ),
	end_(begin_ + block_size*MAX_BLOCKS)
{
	uint8_t i = 0;
	uint8_t* p = const_cast<uint8_t*>(begin_);
	while( i < MAX_BLOCKS) {
		*p = ++i;
		p += block_size;
	}
}

page::~page() BOOST_NOEXCEPT_OR_NOTHROW {
	delete [] const_cast<uint8_t*>(begin_);
}

uint8_t* page::allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
	uint8_t *result = NULL;
	if(free_blocks_)
	{
		result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
		position_ = *result;
		--free_blocks_;
	}
	return result;
}

bool page::release_if_in(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
	if( ptr >= begin_ && ptr < end_ )
	{
		assert( (ptr - begin_) % block_size == 0 );
		*(const_cast<uint8_t*>(ptr)) = position_;
		position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
		++free_blocks_;
		return true;
	}
	return false;
}

//fixed_allocator
fixed_allocator::fixed_allocator(const std::size_t size):
	size_(size),
	alloc_current_(NULL),
	free_current_(NULL)
{
	// pre cache pages peer cpu
	unsigned int cpus = boost::thread::hardware_concurrency();
	page *pg;
	for(unsigned int i=0; i < cpus; i++)
	{
		pg = new page(size_);
		pages_.push_front( pg );
	}
	alloc_current_.store(pages_.front(), boost::memory_order_relaxed);
	free_current_.store(pages_.front(), boost::memory_order_relaxed);
}

fixed_allocator::~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	pages_list::iterator it = pages_.begin();
	pages_list::iterator end = pages_.end();
	while( it != end )
	{
		delete *it;
		++it;
	}
}

void fixed_allocator::collect() BOOST_NOEXCEPT_OR_NOTHROW
{
	boost::unique_lock<boost::mutex> guard(mtx_);
	pages_list::const_iterator it = pages_.cbegin();
	pages_list::const_iterator end = pages_.cend();
	unsigned int cpus = boost::thread::hardware_concurrency();
	std::size_t release_count = 0;
	while(it != end) {
		page* pg = *it;
		if( pg->is_empty() ) {
			if(++release_count > cpus) {
				delete pg;
				pages_.erase(it);
			}
		}
		++it;
	}
}

void* fixed_allocator::malloc BOOST_PREVENT_MACRO_SUBSTITUTION() const
{
	boost::unique_lock<boost::mutex> guard(mtx_);
	page *pg = alloc_current_;
	uint8_t *result = pg->allocate(size_);
	if(!result) {
		pages_list::const_iterator it = pages_.cbegin();
		pages_list::const_iterator end = pages_.cend();
		// if previws failed, try to allocate from an exising page
		while( it != end ) {
			pg = (*it);
			assert(pg);
			result = pg->allocate(size_);
			if(result) break;
			++it;
		}
	}
	if(!result) {
		// no more space left, or all pages where locked
		// allocate a new page from system
		pg =  new page(size_);
		result = pg->allocate(size_);
		pages_.push_front( pg );
	}
	alloc_current_.store(pg, boost::memory_order_seq_cst);
	return static_cast<void*>(result);
}

void fixed_allocator::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) const BOOST_NOEXCEPT_OR_NOTHROW
{
	boost::unique_lock<boost::mutex> guard(mtx_);
	const uint8_t *p = static_cast<const uint8_t*>(ptr);
	page *pg = free_current_.load(boost::memory_order_relaxed);
	if( !pg->release_if_in(p, size_) ) {
		pages_list::const_iterator it = pages_.cbegin();
		pages_list::const_iterator end = pages_.cend();
		for(;;) {
			pg = *it;
			if( pg->release_if_in(p, size_) ) {
				break;
			}
			++it;
			if(it == end) {
				boost::this_thread::yield();
				it = pages_.cbegin();
			}
		}
		free_current_.store(pg, boost::memory_order_seq_cst);
	}
}

// allign to the size_t, i.e. on 4 for 32 bit and 8 for for 64 bit
static BOOST_FORCEINLINE std::size_t align_up(const std::size_t alignment,const std::size_t size)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

// object_allocator
static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(object) ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT;

object_allocator* const object_allocator::instance()
{
	object_allocator *tmp = _instance.load(boost::memory_order_consume);
	if (!tmp) {
		boost::mutex::scoped_lock lock(_mtx);
		tmp = _instance.load(boost::memory_order_consume);
		if (!tmp) {
			tmp = new object_allocator();
			_instance.store(tmp, boost::memory_order_release);
			std::atexit(&object_allocator::release);
		}
	}
	return tmp;
}

void* object_allocator::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
{
	const pool_t* p = pool(size);
	assert(p);
	return p->malloc();
}

void object_allocator::free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
	const pool_t* p = pool(size);
	assert(p);
	p->free(ptr);
}

object_allocator::~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	cv_.notify_all();
	pool_t* p = const_cast<pool_t*>(pools_);
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
	{
		p->~pool_t();
		++p;
	}
	std::free( const_cast<pool_t*>(pools_) );
}


BOOST_FORCEINLINE object_allocator::pool_t* object_allocator::create_pools()
{
	return static_cast<pool_t*>(std::malloc( POOLS_COUNT * sizeof(pool_t) ) );
}

void object_allocator::gc() {
	while( !exit_) {
		boost::this_thread::sleep_for( boost::chrono::secconds(10) );
		pool_t *p = pools_;
		for(uint8_t i = 0; i < POOLS_COUNT ; i++ ) {
			p->collect();
			++p;
		}
	}
}

object_allocator::object_allocator():
	pools_( create_pools() )
{
	pool_t* p = const_cast<pool_t*>(pools_);
	std::size_t pool_size = MIN_SIZE;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ ) {
		p = new (p) pool_t(pool_size);
		pool_size += sizeof(std::size_t);
		++p;
	}
}

BOOST_FORCEINLINE const object_allocator::pool_t* object_allocator::pool(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
	std::size_t index = ( align_up(sizeof(std::size_t),size) / sizeof(std::size_t) ) - SHIFT;
	return pools_ + index;
}

void object_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	delete _instance.load(boost::memory_order_consume);
}

boost::mutex object_allocator::_mtx;
boost::atomic<object_allocator*> object_allocator::_instance(NULL);

} // namespace detail


// object
object::object() BOOST_NOEXCEPT_OR_NOTHROW:
ref_count_(0)
{}

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

} // namespace smallobject

} // namespace boost
