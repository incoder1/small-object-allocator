#include "object.hpp"

namespace boost {

namespace smallobject {

namespace detail {

class page: private boost::noncopyable {
public:
	static const uint8_t MAX_BLOCKS = static_cast<uint8_t>(UCHAR_MAX);

	explicit page(const uint8_t block_size):
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

	~page() BOOST_NOEXCEPT_OR_NOTHROW {
		delete [] const_cast<uint8_t*>(begin_);
	}

	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole page
	 * \param blocks count of blocks to be allocated in time
	 */
	uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
	{
		uint8_t *result = NULL;
		if(free_blocks_) {
			boost::unique_lock<boost::mutex> guard(this->mtx_, boost::try_to_lock );
			if( guard.owns_lock() && free_blocks_ ) {
				result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
				position_ = *result;
				--free_blocks_;
			}
		}
		return result;
	}

	/**
	 * Releases previusly allocated memory if pointer is from this page
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of minimal allocated block
	 */
	bool release_if_in(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
		if( is_pointer_in(ptr) ) {
			boost::unique_lock<boost::mutex> guard(this->mtx_);
			assert( (ptr - begin_) % block_size == 0 );
			*(const_cast<uint8_t*>(ptr)) = position_;
			position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
			++free_blocks_;
			return true;
		}
		return false;
	}
private:
	inline bool is_pointer_in(const uint8_t* ptr)
	{
		return ptr >= begin_ && ptr < end_;
	}
private:
	uint8_t position_;
	uint8_t free_blocks_;
	const uint8_t* begin_;
	const uint8_t* end_;
	boost::mutex mtx_;
};

static uint32_t available_logical_cpus()
{
#ifdef BOOST_WINDOWS_API
	::SYSTEM_INFO sysinfo;
	::GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif defined(BOOST_POSIX_API)
	return ::sysconf( _SC_NPROCESSORS_ONLN );
#else
	return 1UL;
#endif // platform select
}


/**
 * \brief Allocates only a signle memory block of the same size
 */
class fixed_allocator:private boost::noncopyable {
private:
	/// Vector of memory block pages
	typedef lockfree::forward_list<page*> pages_list;
public:

	/// Constructs new fixed allocator of specific block size
	explicit fixed_allocator(const std::size_t size):
		size_(size),
		alloc_current_(NULL),
		free_current_(NULL)
	{
		page* first = new page(size);
		pages_.push_front(first);
		alloc_current_.store(first, boost::memory_order_relaxed);
		free_current_.store(first, boost::memory_order_relaxed);
		// pre cache pages for available CPUS
		uint32_t precache = available_logical_cpus() - 1;
		for(uint32_t i=0; i < precache; i++) {
			pages_.push_front(new page(size) );
		}
	}

	/// releases allocator and all allocated memory
	~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
		pages_list::iterator it = pages_.begin();
		pages_list::iterator end = pages_.end();
		while( it != end )
		{
            delete *it;
            ++it;
		}
	}
	/// Allocates a single memory block of fixed size
	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION() const
	{
		page *tmp = alloc_current_;
		assert(tmp);
		uint8_t *result = tmp->allocate(size_);
		if(!result) {
			pages_list::const_iterator it = pages_.cbegin();
			pages_list::const_iterator end = pages_.cend();
		    // if previws failed, try to allocate from an exising page
			while( it != end ) {
				tmp = (*it);
				assert(tmp);
				result = tmp->allocate(size_);
				if(result) break;
				++it;
			};
		}
		if(!result) {
			// no more space left, or all pages where locked
			// allocate a new page from system
			tmp =  new page(size_);
			result = tmp->allocate(size_);
			pages_.push_front( tmp );
		}
		alloc_current_.store(tmp, boost::memory_order_seq_cst);
		return static_cast<void*>(result);
	}
	/**
	 * Releases previesly allocated block of memory
	 * \param ptr pointer to the allocated memory
	 */
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		const uint8_t *p = static_cast<const uint8_t*>(ptr);
		page *tmp = free_current_.load(boost::memory_order_relaxed);
		if( !tmp->release_if_in(p, size_) ) {
			pages_list::const_iterator it = pages_.cbegin();
			pages_list::const_iterator end = pages_.cend();
			for(;;) {
				tmp = *it;
				if( tmp->release_if_in(p, size_) ) {
					break;
				}
				++it;
				if(it == end) {
					boost::this_thread::yield();
					it = pages_.cbegin();
				}
			}
			free_current_.store(tmp, boost::memory_order_seq_cst);
		}
	}

private:
	const std::size_t size_;
	mutable boost::atomic<page*>  alloc_current_;
	mutable boost::atomic<page*>  free_current_;
	mutable pages_list pages_;
};


// object_allocator
class object_allocator:private boost::noncopyable  {
private:
	typedef fixed_allocator pool_t;
	typedef std::vector<const pool_t*> pools_vector;
	static const std::size_t MIN_SIZE = sizeof(object);
	static const std::size_t MAX_SIZE = 128; // max size is 128
public:

	static BOOST_FORCEINLINE object_allocator* const instance()
	{
		object_allocator  *tmp = _instance.load(boost::memory_order_consume);
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

	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
	{
		void * result = NULL;
		if( size <= MAX_SIZE ) {
			const pool_t* p = pool(size);
			assert(p);
			result = p->malloc();
		} else {
			result = static_cast<void*>( new uint8_t[size] );
		}
		return result;
	}

	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size)
	{
		if(size > MAX_SIZE) {
			delete [] static_cast<uint8_t*>(ptr);
		} else {
			const pool_t* p = pool(size);
			assert(p);
			p->free(ptr);
		}
	}

	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW {
		pools_vector::const_iterator it = pools_.cbegin();
		pools_vector::const_iterator end = pools_.cend();
		while(it != end)
		{
			delete const_cast<pool_t*>(*it);
			++it;
		}
	}

private:

	object_allocator()
	{
		std::size_t size = MAX_SIZE / MIN_SIZE;
		std::size_t pool_size = MIN_SIZE;
		for(uint8_t i = 0; i < size ; i++ ) {
			pools_.push_back( new pool_t(pool_size) );
			pool_size += MIN_SIZE;
		}
	}

	inline std::size_t align_up(const std::size_t alignment,const std::size_t size) {
		return (size + alignment - 1) & ~(alignment - 1);
	}

	inline const pool_t* pool(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW {
		std::size_t index = align_up(MIN_SIZE,size) / MIN_SIZE - 1;
		return pools_[index];
	}

	static void release() BOOST_NOEXCEPT_OR_NOTHROW {
		delete _instance.load(boost::memory_order_consume);
	}

private:
	static boost::mutex _mtx;
	static boost::atomic<object_allocator*> _instance;
	pools_vector pools_;
};

boost::mutex object_allocator::_mtx;
boost::atomic<object_allocator*> object_allocator::_instance(NULL);

} // namespace detail


// object
object::object() BOOST_NOEXCEPT_OR_NOTHROW:
	ref_count_(0)
{}

object::~object() BOOST_NOEXCEPT_OR_NOTHROW
{}

void* object::operator new(const std::size_t s) BOOST_THROWS(std::bad_alloc)
{
	void* result = detail::object_allocator::instance()->malloc(s);
	if(!result) {
		boost::throw_exception( std::bad_alloc() );
	}
	return result;
}

void object::operator delete(void *ptr,const std::size_t s) BOOST_NOEXCEPT_OR_NOTHROW {
	detail::object_allocator::instance()->free(ptr,s);
}

void intrusive_ptr_add_ref(object* obj)
{
	obj->ref_count_.fetch_add(1, boost::memory_order_relaxed);
}

void intrusive_ptr_release(object* const obj)
{
	if (obj->ref_count_.fetch_sub(1, boost::memory_order_release) == 1) {
		boost::atomics::atomic_thread_fence(boost::memory_order_acquire);
		delete obj;
	}
}

} // namespace smallobject

} // namespace boost
