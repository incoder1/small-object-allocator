#include "object.hpp"

namespace boost {

#if defined(BOOST_NO_EXCEPTIONS)
void throw_exception(std::exception const & e)
{
	std::cerr<< e.what();
	std::exit(-1);
}
#endif // BOOST_NO_EXCEPTIONS

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
	end_(begin + (MAX_BLOCKS * block_size) ),
	prev_(NULL),
	next_(NULL)
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
const unsigned int arena::CPUS = boost::thread::hardware_concurrency();

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

arena::arena(const std::size_t size, const pool* owner):
	block_size_(size),
	root_( create_new_chunk(block_size_) ),
	alloc_current_(root_),
	free_current_(root_),
	owner_(owner)
{}

arena::~arena() BOOST_NOEXCEPT_OR_NOTHROW
{
  chunk *chnk = root_;
  while(NULL != chnk) {
	 chnk = chnk->next();
	 if(NULL != chnk) {
		release_chunk(chnk, block_size_);
	 }
  }
  release_chunk(root_, block_size_);
}

bool arena::empty() BOOST_NOEXCEPT_OR_NOTHROW
{
	bool result = false;
	chunk *chnk = root_;
	while(NULL != chnk) {
		chnk = chnk->next();
		if(NULL != chnk) {
			boost::unique_lock<spin_lock> lock(mtx_);
			result = chnk->empty();
			if(!result) break;
		}
   }
   return result;
}

inline void* arena::malloc BOOST_PREVENT_MACRO_SUBSTITUTION() BOOST_THROWS(std::bad_alloc)
{
	chunk* chnk = alloc_current_;
	uint8_t* result = NULL;
	// try to allocate from current chunk
	{
		boost::unique_lock<spin_lock> lock(mtx_);
		result = chnk->allocate(block_size_);
	}
	if(!result) {
		// try to allocate from existing chunks
		chnk = chnk->next();
		while(chnk) {
			boost::unique_lock<spin_lock> lock(mtx_);
			result = chnk->allocate(block_size_);
			if(result) {
				alloc_current_ = chnk;
				break;
			}
			chnk = chnk->next();
		}
		// no space left, create new chunk
		if(!result) {
			boost::unique_lock<spin_lock> lock(mtx_);
			chunk* new_chunk = create_new_chunk(block_size_);
			new_chunk->set_prev(chnk);
			chnk->set_next(chnk);
			alloc_current_ = new_chunk;
			free_current_ = new_chunk;
		}
	}
	return static_cast<void*>(result);
}


inline bool arena::try_to_free(const uint8_t* ptr, chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	boost::unique_lock<spin_lock> lock(mtx_);
	return cnk->release(ptr,block_size_);
}

bool arena::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	const uint8_t *rptr = static_cast<const uint8_t*>(ptr);
	chunk* chnk = free_current_;
	bool result = try_to_free(rptr, chnk);
	if(!result) {
		chnk = root_;
		std::size_t count = 0;
		while(NULL != chnk) {
            chnk = chnk->next();
			result = try_to_free(rptr, chnk);
			if(result) {
				break;
			}
			// if emty inc count for shrinking
			if(chnk->empty()) ++count;
		}
		// shrink chunk if empty, and there are anoeth empty chunks
		if(result && (count > CPUS) && chnk->empty() ) {
			boost::unique_lock<spin_lock> lock(mtx_);
			if( chnk->empty() ) {
				chnk->prev()->set_next( chnk->next() );
				release_chunk(chnk, block_size_);
			}
		}
	}
	return result;
}

// poll
pool::pool(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW:
    arena_( &pool::release_arena )
{}

void pool::release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW
{
	pool* this_pool = const_cast<pool*>(ar->owner());
	this_pool->erase_arena(ar);
}

inline void pool::erase_arena(arena* ar)
{
	if( ar->empty() ) {
		// don't hold lock when releasing arena memory
		{
			boost::unique_lock<spin_lock> lock(mtx_);
			arenas_list::iterator pos = std::find(all_arenas_.begin(), all_arenas_.end(), ar);
			all_arenas_.erase(pos);
		}
		delete ar;
	}
}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
	// release all arenas with deallocation thread thread alloc/free missmatch
	// or with memory leak
	arenas_list::const_iterator it = all_arenas_.cbegin();
	arenas_list::const_iterator end = all_arenas_.cend();
	while(it != end) {
		delete *it;
		++it;
	}
}

inline arena* pool::get_arena(const std::size_t size)
{
    arena *result = arena_.get();
    if(!result) {
		result = new arena(size, this);
		arena_.reset(result);
		boost::unique_lock<spin_lock> lock(mtx_);
		all_arenas_.push_front(result);
    }
    return result;
}

BOOST_FORCEINLINE void *pool::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
{
    return get_arena(size)->malloc();
}

BOOST_FORCEINLINE void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
    bool released = arena_->free(ptr);
    // if memory allocated from another thread (thread missmatch)
    // loop all arenas and release this memory block
    if(!released) {
		arenas_list::const_iterator it = all_arenas_.cbegin();
		arenas_list::const_iterator end = all_arenas_.cend();
		while(!released) {
			released = (*it)->free(ptr);
			if(++it == end) {
				// yield this thread
				// since allocating thread updating all_arenas_ list
				// may happens in case of good hardware concurrency
				boost::this_thread::yield();
				it = all_arenas_.cbegin();
			}
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
		boost::lock_guard<spin_lock> lock(_smtx);
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
		p = new (p) pool_t(pool_size);
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
	delete _instance.load(boost::memory_order_consume);
}

spin_lock object_allocator::_smtx;
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
