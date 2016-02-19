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
	if(free_blocks_)
	{
        boost::unique_lock<spin_lock> guard(mtx_, boost::try_to_lock);
        if( guard.owns_lock() && free_blocks_ )  {
            result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
            position_ = *result;
            --free_blocks_;
		}
	}
	return result;
}

BOOST_FORCEINLINE bool chunk::release(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
{
    if( ptr >= begin_ && ptr < end_ ) {
        assert( (ptr - begin_) % block_size == 0 );
        boost::unique_lock<spin_lock> lock(mtx_);
        *(const_cast<uint8_t*>(ptr)) = position_;
        position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
        ++free_blocks_;
        return true;
    }
    return false;
}

//arena
const unsigned int arena::CPUS = boost::thread::hardware_concurrency();

arena::arena(const std::size_t size):
	alloc_current_(arena::no_free_f ),
	free_current_(arena::no_free_f)
{
	chunk *c = NULL;
	for(unsigned int i=0; i < CPUS; i++) {
		c = create_new_chunk(size);
		chunks_.push_front(c);
	}
	alloc_current_.reset(c);
}

BOOST_FORCEINLINE chunk* arena::create_new_chunk(const std::size_t size) const
{
	register void *ptr = sys::xmalloc( sizeof(chunk) + (size * chunk::MAX_BLOCKS) );
	const uint8_t *begin = static_cast<uint8_t*>(ptr) + sizeof(chunk);
	return new (ptr) chunk(size, begin);
}

BOOST_FORCEINLINE void arena::release_chunk(chunk* const cnk) const BOOST_NOEXCEPT_OR_NOTHROW
{
	assert(cnk);
	cnk->~chunk();
	sys::xfree( static_cast<void*>(cnk) );
}

arena::~arena() BOOST_NOEXCEPT_OR_NOTHROW
{
	chunks_list::iterator it = chunks_.begin();
	chunks_list::iterator end = chunks_.end();
	while(it != end)
	{
		release_chunk(*it);
		++it;
	}
}

inline void* arena::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) BOOST_THROWS(std::bad_alloc)
{
	chunk* cnk = alloc_current_.get();
	uint8_t *result = cnk ? cnk->allocate(size) :  NULL;
	if(!result) {
		chunks_list::const_iterator it = chunks_.cbegin();
		chunks_list::const_iterator end = chunks_.cend();
		// if previws failed, try to allocate from an exising chunk
		while(it != end) {
			cnk = (*it);
			result = cnk->allocate(size);
			if(result) {
				break;
			}
			++it;
		}
		if(result) {
			alloc_current_.reset(cnk);
		}
	}
	if(!result) {
        // no space left, or all memory chunks were locked by another threads
		cnk =  create_new_chunk(size);
		result = cnk->allocate(size);
		chunks_.push_front( cnk );
		alloc_current_.reset(cnk);
		free_current_.reset(cnk);
	}
	return static_cast<void*>(result);
}

inline void arena::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr,const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	const uint8_t *p = static_cast<const uint8_t*>(ptr);
	chunk *cnk = free_current_.get();
	if( cnk && !cnk->release(p,size) ) {
		chunks_list::const_iterator it = chunks_.cbegin();
		chunks_list::const_iterator prev = chunks_.cbefore_begin();
		chunks_list::const_iterator end = chunks_.cend();
		std::size_t lcount = 0;
		while(true) {
            cnk = *it;
			if ( cnk->release(p,size) ) {
				// shrinking if needed
				if( lcount > CPUS && cnk->empty() ) {
					if( cnk->try_lock() ) {
						if( cnk->empty() ) {
							chunks_.pop_after(prev);
							release_chunk(cnk);
						} else {
							cnk->unlock();
						}
					}
				}
                break;
			}
			++lcount;
			++prev;
            if(++it == end) {
                lcount = 0;
                it = chunks_.cbegin();
                prev = chunks_.cbefore_begin();
            }
    	}
		free_current_.reset(cnk);
	}
}

// poll
spin_lock pool::_mtx;

pool::pool(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW:
    arena_(NULL),
    block_size_(block_size)
{
}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
   register arena* ar = arena_.load(boost::memory_order_relaxed);
   if( ar ) {
        delete ar;
   }
}

inline arena* pool::get_arena()
{
    arena *result = arena_.load(boost::memory_order_relaxed);
    if(!result) {
        boost::unique_lock<spin_lock> lock(_mtx);
        result = arena_.load(boost::memory_order_acquire);
        if(!result) {
            result = new arena(block_size_);
            arena_.store(result, boost::memory_order_release);
        }
    }
    return result;
}

BOOST_FORCEINLINE void *pool::malloc BOOST_PREVENT_MACRO_SUBSTITUTION()
{
    return get_arena()->malloc(block_size_);
}

BOOST_FORCEINLINE void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
    arena *all = arena_.load(boost::memory_order_relaxed);
    all->free(ptr, block_size_);
}

// object_allocator

static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(object) ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT;


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
	return p->malloc();
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
