#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include <cstddef>
#include <new>
#include <list>

#include <boost/atomic.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#ifndef BOOST_THROWS
#	ifdef BOOST_NO_CXX11_NOEXCEPT
#		define BOOST_THROWS(_EXC) throw(_EXC)
#	else
#		define BOOST_THROWS(_EXC)
#	endif  // BOOST_NO_CXX11_NOEXCEPT
#endif // BOOST_THROWS

namespace boost {

namespace smallobject {

// 64 for 32 bit and 128 for 64 bit instructions
extern BOOST_SYMBOL_VISIBLE const std::size_t MAX_SIZE;

namespace detail {

/**
 * ! \brief A chunk of allocated memory divided on UCHAR_MAX of fixed blocks
 */
class page {
BOOST_MOVABLE_BUT_NOT_COPYABLE(page)
public:
	explicit page(const uint8_t block_size);
	~page() BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole page
	 * \param blocks count of blocks to be allocated in time
	 */
	uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Releases previusly allocated memory if pointer is from this page
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of minimal allocated block
	 */
	bool release_if_in(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;

	BOOST_FORCEINLINE bool is_empty() {
		return free_blocks_ == MAX_BLOCKS;
	}

private:
	static const uint8_t MAX_BLOCKS;
	uint8_t position_;
	uint8_t free_blocks_;
	const uint8_t* begin_;
	const uint8_t* end_;
};

class spin_lock {
BOOST_MOVABLE_BUT_NOT_COPYABLE(spin_lock)
private:
	enum lock_state {LOCKED, UNLOCKED};
public:
	spin_lock():
		state_(UNLOCKED)
	{}

	void lock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		std::size_t spin_count = 0;
		while( ! state_.exchange(LOCKED, boost::memory_order_consume) )
		{
			boost::this_thread::interruptible_wait(++spin_count);
		}
	}

	void unlock() BOOST_NOEXCEPT_OR_NOTHROW
	{
		state_.store(UNLOCKED, boost::memory_order_release);
	}
private:
	boost::atomic<lock_state> state_;
};


/**
 * \brief Allocates only a signle memory block of the same size
 */
class fixed_allocator {
BOOST_MOVABLE_BUT_NOT_COPYABLE(fixed_allocator)
private:
	/// Vector of memory block pages
	typedef std::list<page*> pages_list;
public:
	/// Constructs new fixed allocator of specific block size
	explicit fixed_allocator(const std::size_t size);
	/// releases allocator and all allocated memory
	~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
	/// Allocates a single memory block of fixed size
	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION() const;
	/**
	* Releases previesly allocated block of memory
	* \param ptr pointer to the allocated memory
	*/
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) const BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Collect empty pages, i.e. collect garbage
	 */
	void collect() const BOOST_NOEXCEPT_OR_NOTHROW;
private:
	const std::size_t size_;
	mutable boost::atomic<page*> alloc_current_;
	mutable boost::atomic<page*> free_current_;
	mutable pages_list pages_;
	mutable spin_lock mtx_;
};

/**
 * ! \brief Allocates memory for the small objects
 *  maximum size of small object is sizeof(size_type) * 16
 */
class object_allocator:private boost::noncopyable
{
private:
	typedef fixed_allocator pool_t;
public:
	static object_allocator* const instance();
	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size);
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static BOOST_FORCEINLINE pool_t* create_pools();
	explicit object_allocator();
	BOOST_FORCEINLINE const pool_t* pool(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
	void gc() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static spin_lock _smtx;
	static boost::atomic<object_allocator*> volatile _instance;
	boost::mutex mtx_;
	boost::condition_variable cv_;
	boost::atomic_bool exit_;
	pool_t* pools_;
};

} // namespace detail

class BOOST_SYMBOL_VISIBLE object
{
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      object( const object& ) = delete;
      object& operator=( const object& ) = delete;
#else
  private:  // emphasize the following members are private
      object( const object& );
      object& operator=( const object& );
#endif // no deleted functions
protected:
	object() BOOST_NOEXCEPT_OR_NOTHROW;
public:
	virtual ~object() BOOST_NOEXCEPT_OR_NOTHROW = 0;
	// redefine new and delete operations for small object optimized memory allocation
	void* operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc)
	{
		void* result = NULL;
		if(bytes <= MAX_SIZE) {
			result = detail::object_allocator::instance()->malloc(bytes);
		} else {
			result = std::malloc(bytes);
		}
		if( ! result ) {
#ifndef BOOST_NO_EXCEPTIONS
			throw std::bad_alloc();
#else
			std::new_handler();
#endif // BOOST_NO_EXCEPTIONS
		}
		return result;
	}
	void operator delete(void *ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW
	{
		if(bytes <= MAX_SIZE) {
			detail::object_allocator::instance()->free(ptr, bytes);
		} else {
			std::free(ptr);
		}
	}
private:
	// ass support for boost::intrusive_ptr
	boost::atomics::atomic_size_t ref_count_;
	friend void intrusive_ptr_add_ref(object* const obj);
	friend void intrusive_ptr_release(object* const obj);
};

// support of boost::intrusive_ptr
BOOST_FORCEINLINE void intrusive_ptr_add_ref(object* const obj)
{
	obj->ref_count_.fetch_add(1, boost::memory_order_relaxed);
}

BOOST_FORCEINLINE void intrusive_ptr_release(object* const obj)
{
	if (obj->ref_count_.fetch_sub(1, boost::memory_order_release) == 1) {
		boost::atomics::atomic_thread_fence(boost::memory_order_acquire);
		delete obj;
	}
}

} // namespace smallobject

using smallobject::object;

#ifndef BOOST_DECLARE_OBJECT_PTR_T
#	define BOOST_DECLARE_OBJECT_PTR_T(TYPE) typedef boost::intrusive_ptr<TYPE> s_##TYPE
#endif // DECLARE_SPTR_T

} // namespace boost

#endif // __LIBGC_OBJECT_HPP_INCLUDED__
