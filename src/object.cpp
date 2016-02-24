#include "object.hpp"

namespace boost {

// allign to the size_t, i.e. on 4 for 32 bit and 8 for for 64 bit
static BOOST_FORCEINLINE std::size_t align_up(const std::size_t alignment,const std::size_t size) BOOST_NOEXCEPT
{
	return (size + alignment - 1) & ~(alignment - 1);
}

namespace smallobject {

const std::size_t MAX_SIZE = sizeof(std::size_t) * 16;

namespace detail {


chunk::chunk(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW:
	position_(0),
	free_blocks_(MAX_BLOCKS),
	begin_( begin ),
	end_(begin + (MAX_BLOCKS * block_size) )
{
	register uint8_t i = 0;
	uint8_t* p = const_cast<uint8_t*>(begin_);
	while( i < MAX_BLOCKS) {
		*p = ++i;
		p += block_size;
	}
}

BOOST_FORCEINLINE uint8_t* chunk::allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
	register uint8_t *result = NULL;
	if(free_blocks_) {
		result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
		position_ = *result;
		--free_blocks_;
	}
	return result;
}

BOOST_FORCEINLINE bool chunk::release(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
{
	register bool result = ptr >= begin_ && ptr < end_;
    if( result ) {
        *(const_cast<uint8_t*>(ptr)) = position_;
        position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
        ++free_blocks_;
        return true;
    }
    return result;
}

//arena
BOOST_FORCEINLINE chunk* arena::create_new_chunk(const std::size_t size)
{
	void *ptr = sys::xmalloc( sizeof(chunk) + (size * chunk::MAX_BLOCKS) );
	const uint8_t *begin = static_cast<uint8_t*>(ptr) + sizeof(chunk);
	return new (ptr) chunk(size, begin);
}

BOOST_FORCEINLINE void arena::release_chunk(chunk* const cnk, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	assert(cnk);
	cnk->~chunk();
	sys::xfree( static_cast<void*>(cnk), size + sizeof(chunk) );
}

arena::arena(const std::size_t block_size):
	block_size_(block_size),
	chunks_(),
	alloc_current_(NULL),
	free_current_(NULL),
	reserved_(false)
{
	chunk* first = create_new_chunk( block_size_ );
	alloc_current_ = first;
	free_current_  = first;
	chunks_.push_back(first);
}

arena::~arena() BOOST_NOEXCEPT_OR_NOTHROW
{
	for(vchunks::iterator it= chunks_.begin(); it != chunks_.end(); ++it) {
		release_chunk(*it, block_size_ );
	}
}

BOOST_FORCEINLINE uint8_t* arena::try_to_alloc(chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	boost::unique_lock<sys::critical_section> lock(mtx_);
	return cnk->allocate(block_size_);
}

void* arena::malloc BOOST_PREVENT_MACRO_SUBSTITUTION() BOOST_THROWS(std::bad_alloc)
{
	chunk* current = alloc_current_;
	uint8_t* result = try_to_alloc(current);
	if(!result) {
		vchunks::const_iterator it = chunks_.cbegin();
		while( it != chunks_.cend() ) {
			current = *it;
			result = try_to_alloc(current);
			if(result) {
				alloc_current_ = current;
				break;
			}
			++it;
		}
	}
	// no space left, create new chunk
	if(!result) {
		current = create_new_chunk(block_size_);
		result = current->allocate(block_size_);
		boost::unique_lock<sys::critical_section> lock(mtx_);
		chunks_.push_front(current);
		alloc_current_ = current;
		free_current_ = current;
	}
	return static_cast<void*>(result);
}

inline bool arena::try_to_free(const uint8_t* ptr, chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	boost::unique_lock<sys::critical_section> lock(mtx_);
	bool result = cnk->release(ptr,block_size_);
	if(result) free_current_ = cnk;
	return result;
}

bool arena::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	const uint8_t *p = static_cast<const uint8_t*>(ptr);
	chunk* current = free_current_;
	bool result = try_to_free(p, current);
	std::size_t empties = 0;
	if(!result) {
		vchunks::const_iterator it = chunks_.begin();
		while( it != chunks_.end() ) {
			current = *it;
			if(current->empty()) ++empties;
			result = try_to_free(p, current);
			if(result) {
				break;
			}
			++it;
		}
		// neeed shrink
		if(empties > 2 ) {
			shrink();
		}
	}
	return result;
}

void arena::shrink() BOOST_NOEXCEPT_OR_NOTHROW
{
	vchunks shinks;
	std::size_t empties = 0;
	{
		vchunks::iterator it= chunks_.begin();
		do  {
			if( (*it)->empty() && ++empties > 2 ) {
				shinks.push_back(*it);
				vchunks::iterator tmp = it;
				++it;
				boost::unique_lock<sys::critical_section> lock(mtx_);
				chunks_.erase(tmp);
			} else {
				++it;
			}
		} while(it != chunks_.end() );
		boost::unique_lock<sys::critical_section> lock(mtx_);
		alloc_current_ = *chunks_.begin();
		free_current_ = alloc_current_;
	}
	for(vchunks::const_iterator it= shinks.cbegin(); it != shinks.cend(); ++it) {
		release_chunk(*it, block_size_);
	}
}

// poll

inline void pool::release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW
{
	ar->release();
}

pool::pool() BOOST_NOEXCEPT_OR_NOTHROW:
	arena_(&pool::release_arena),
	mtx_()
{}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
	for(arenas_pool::const_iterator it= arenas_.cbegin(); it != arenas_.cend(); ++it) {
		delete *it;
	}
}

inline arena* pool::reserve(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	arena *result = arena_.get();
	if(!result) {
		arenas_pool::const_iterator it = arenas_.cbegin();
		arenas_pool::const_iterator end = arenas_.cend();
		while(it != end) {
            bool reserved = (*it)->reserve();
			if(reserved) {
				result = *it;
				break;
			}
			++it;
		}
		if(!result) {
			result = new arena(size);
			boost::unique_lock<sys::critical_section> lock(mtx_);
			arenas_.push_front(result);
		}
		arena_.reset(result);
	}
	return result;
}

void* pool::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
{
	arena *ar = reserve(size);
	return ar->malloc();
}

BOOST_FORCEINLINE void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	bool released = arena_->free(ptr);
	// handle allocation from another thread
	while(!released) {
		arenas_pool::const_iterator it = arenas_.cbegin();
		arenas_pool::const_iterator end = arenas_.cend();
		while(it != end) {
			assert(*it);
			released = (*it)->free(ptr);
			++it;
		}
		if(!released) {
			// allocated by another CPU running thread not while ago
			boost::this_thread::yield();
		}
	}
}

// object_allocator

static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(object) ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT; // count of small object pools = 14


const object_allocator* object_allocator::instance()
{
	object_allocator *tmp = _instance.load(boost::memory_order_relaxed);
	if (!tmp) {
		boost::lock_guard<sys::critical_section> lock(_smtx);
		tmp = _instance.load(boost::memory_order_acquire);
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
	register pool_t* p = pools_;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
	{
		p->~pool_t();
		++p;
	}
	sys::xfree(pools_);
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

} // namespace detail


// object
object::object() BOOST_NOEXCEPT_OR_NOTHROW:
	ref_count_(0)
{}

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

void* object::operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc)
{
	register void* result = NULL;
	if(bytes <= MAX_SIZE) {
		result = detail::object_allocator::instance()->malloc(bytes);
		if( ! result ) {
			boost::throw_exception( std::bad_alloc() );
		}
	} else {
		result = ::operator new(bytes);
	}
	return result;
}

void object::operator delete(void* const ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW {
	if(bytes <= MAX_SIZE) {
		detail::object_allocator::instance()->free(ptr, bytes);
	} else {
		::operator delete(ptr);
	}
}

} // namespace smallobject

} // namespace boost
