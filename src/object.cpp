#include "object.hpp"

namespace boost {

#ifdef BOOST_NO_EXCEPTIONS

void throw_exception(std::exception const & e)
{
	std::new_handler();
}

#endif // BOOST_NO_EXCEPTIONS

namespace smallobject {

const std::size_t MAX_SIZE = sizeof(std::size_t) * 16;

namespace detail {

// page
const uint8_t page::MAX_BLOCKS = static_cast<uint8_t>(UCHAR_MAX);

page::page(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW:
	position_(0),
	free_blocks_(MAX_BLOCKS),
	begin_( begin ),
	end_(begin + (MAX_BLOCKS * block_size) )
{
	uint8_t i = 0;
	uint8_t* p = const_cast<uint8_t*>(begin_);
	while( i < MAX_BLOCKS) {
		*p = ++i;
		p += block_size;
	}
}

#if defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) || defined(BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS)
page::~page() BOOST_NOEXCEPT_OR_NOTHROW
{}
#endif // default destructor

inline uint8_t* page::allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
	uint8_t *result = NULL;
	if(free_blocks_)
	{
		result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
		position_ = *result;
		--free_blocks_;
	}
	return result;
}

inline bool page::release_if_in(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
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
fixed_allocator::fixed_allocator(const std::size_t block_size):
	alloc_current_(NULL),
	free_current_(NULL)
{
	page* pg = create_new_page(block_size);
	alloc_current_.store(pg, boost::memory_order_relaxed);
	free_current_.store(pg, boost::memory_order_relaxed);
}

BOOST_FORCEINLINE page* fixed_allocator::create_new_page(const std::size_t size) const
{
	uint8_t *ptr = new uint8_t[sizeof(page) + size * page::MAX_BLOCKS ];
	return new (static_cast<void*>(ptr)) page(size, ptr+sizeof(page) );
}

BOOST_FORCEINLINE void fixed_allocator::release_page(page* const pg) const BOOST_NOEXCEPT_OR_NOTHROW
{
	assert(pg);
	pg->~page();
	delete [] reinterpret_cast<uint8_t*>(pg);
}

fixed_allocator::~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	pages_list::iterator it = pages_.begin();
	pages_list::iterator end = pages_.end();
	for( ;it != end; ++it )
	{
		release_page(*it);
	}
}

void fixed_allocator::collect() BOOST_NOEXCEPT_OR_NOTHROW {
		pages_list::iterator it = pages_.begin();
		pages_list::iterator end = pages_.end();
		while(it != end) {
			page* pg = *it;
			if( pg->is_empty() && alloc_current_.load() == pg ) {
				boost::unique_lock<boost::mutex> guard(mtx_, boost::try_to_lock);
				if( guard.owns_lock() ) {
					release_page(pg);
					pages_.erase(it);
				}
			}
			++it;
		}
}

void* fixed_allocator::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) BOOST_THROWS(std::bad_alloc)
{
	uint8_t *result = NULL;
	boost::unique_lock<boost::mutex> guard(mtx_, boost::try_to_lock);
	if( guard.owns_lock() ) {
		page *pg = alloc_current_;
		if(pg) {
			result = pg->allocate(size);
		} else {
			// not pages allocated yet, or no space left
			pg =  create_new_page(size);
			result = pg->allocate(size);
			pages_.push_front( pg );
			alloc_current_.store(pg, boost::memory_order_seq_cst);
			free_current_.store(pg, boost::memory_order_seq_cst);
		}
		if(!result) {
			pages_list::iterator it = pages_.begin();
			pages_list::iterator end = pages_.end();
			// if previws failed, try to allocate from an exising page
			while( it != end ) {
				pg = (*it);
				assert(pg);
				result = pg->allocate(size);
				if(result) break;
				++it;
			}
			alloc_current_.store(pg, boost::memory_order_seq_cst);
		}
	}
	return static_cast<void*>(result);
}

bool fixed_allocator::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr,const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
	const uint8_t *p = static_cast<const uint8_t*>(ptr);
	boost::unique_lock<boost::mutex> guard(mtx_);
	page *pg = free_current_.load(boost::memory_order_relaxed);
	bool result = pg->release_if_in(p, size);
	if(!result)
	{
		pages_list::iterator it = pages_.begin();
		pages_list::iterator end = pages_.end();
		while(it != end) {
			pg = *it;
			result = pg->release_if_in(p, size);
			if(result) {
				free_current_.store(pg, boost::memory_order_seq_cst);
				break;
			}
			++it;
		}
	}
	return result;
}

// poll
pool::pool(const std::size_t block_size):
	block_size_(block_size)
{
}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
	fa_list::const_iterator it = allocators_.cbegin();
	fa_list::const_iterator end = allocators_.cend();
	while(it != end) {
		delete *it;
		++it;
	}
}


void *pool::malloc BOOST_PREVENT_MACRO_SUBSTITUTION()
{
	void *result = NULL;
	fa_list::const_iterator it = allocators_.cbegin();
	fa_list::const_iterator end = allocators_.cend();
	while(it != end) {
		result = (*it)->malloc(block_size_);
		if(result) break;
		++it;
	}
	if(!result) {
		fixed_allocator *al = new fixed_allocator(block_size_);
		result = al->malloc(block_size_);
		allocators_.push_front(al);
	}
	return result;
}

void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	fa_list::const_iterator it = allocators_.cbegin();
	fa_list::const_iterator end = allocators_.cend();
	bool released = false;
	while(!released) {
		released = (*it)->free(ptr,block_size_);
		if(++it == end) {
			it = allocators_.cbegin();
		}
	};
}

void pool::collect_unlocked_memory() const BOOST_NOEXCEPT_OR_NOTHROW {
	fa_list::const_iterator it = allocators_.cbegin();
	fa_list::const_iterator end = allocators_.cend();
	while(it != end) {
		(*it)->collect();
		++it;
	}
}

// object_allocator

// allign to the size_t, i.e. on 4 for 32 bit and 8 for for 64 bit
static BOOST_FORCEINLINE std::size_t align_up(const std::size_t alignment,const std::size_t size)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

static const std::size_t MIN_SIZE = align_up( sizeof(std::size_t), sizeof(object) ); // 8 bytes for 32 bit and 16 bytes for 64 bit
static const std::size_t SHIFT = MIN_SIZE / sizeof(std::size_t); // 2 since object size not changed
// 14 pools
static const std::size_t POOLS_COUNT = ( ( MAX_SIZE / sizeof(std::size_t) ) ) - SHIFT;


object_allocator* const object_allocator::instance()
{
	object_allocator *tmp = _instance.load(boost::memory_order_consume);
	if (!tmp) {
		boost::lock_guard<boost::mutex> lock(_smtx);
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
	pools_( NULL )//,
	//gc_thread_( boost::bind(object_allocator::collect_free_memory, this) )
{
	pool_t* p = static_cast<pool_t*>(std::malloc(POOLS_COUNT * sizeof(pool_t) ) );
	pools_= p;
	std::size_t pool_size = MIN_SIZE;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ ) {
		p = new (p) pool_t(pool_size);
		pool_size += sizeof(std::size_t);
		++p;
	}
}

object_allocator::~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
	//gc_thread_.interrupt();
	//gc_thread_.join();
	pool_t* p = pools_;
	for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
	{
		p->~pool_t();
		++p;
	}
	std::free(pools_);
}

void* object_allocator::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
{
	pool_t* p = pool(size);
	assert(p);
	return p->malloc();
}

void object_allocator::free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
	pool_t* p = pool(size);
	assert(p);
	p->free(ptr);
}

void object_allocator::collect_free_memory() const
{
	pool_t* p;
	for(;;) {
		boost::this_thread::interruptible_wait(10000L);
		p = pools_;
		for(uint8_t i = 0; i < POOLS_COUNT ; i++ )
		{
			p->collect_unlocked_memory();
			++p;
		}
	}
}


BOOST_FORCEINLINE object_allocator::pool_t* object_allocator::pool(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
	std::size_t index = ( align_up(sizeof(std::size_t),size) / sizeof(std::size_t) ) - SHIFT;
	return pools_ + index;
}

void object_allocator::release() BOOST_NOEXCEPT_OR_NOTHROW {
	delete _instance.load(boost::memory_order_consume);
}

boost::mutex object_allocator::_smtx;
boost::atomic<object_allocator*> volatile object_allocator::_instance(NULL);

} // namespace detail


// object
object::object() BOOST_NOEXCEPT_OR_NOTHROW:
	ref_count_(0)
{}

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

void* object::operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc)
{
	void* result = NULL;
	if(bytes <= MAX_SIZE) {
		result = detail::object_allocator::instance()->malloc(bytes);
	} else {
		result = ::operator new(bytes);
	}
	if( ! result ) {
		boost::throw_exception( std::bad_alloc() );
	}
	return result;
}

void object::operator delete(void *ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW {
	if(bytes <= MAX_SIZE)
	{
		detail::object_allocator::instance()->free(ptr, bytes);
	} else {
		::operator delete(ptr);
	}
}

} // namespace smallobject

} // namespace boost
