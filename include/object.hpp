#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include <cstddef>
#include <new>

#include "nb_forward_list.hpp"
#include "spinlock.hpp"
#include "sys_allocator.hpp"

#include <boost/intrusive_ptr.hpp>
#include <boost/exception/exception.hpp>
#include <boost/thread/tss.hpp>

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

using smallobject::spin_lock;

/**
 * ! \brief A chunk of allocated memory divided on UCHAR_MAX of fixed blocks
 */
struct page {
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      page( const page& ) = delete;
      page& operator=( const page& ) = delete;
#else
  private:  // emphasize the following members are private
      object( const object& );
      object& operator=( const object& );
#endif // no deleted functions
public:
	BOOST_STATIC_CONSTANT(uint8_t, MAX_BLOCKS  = UCHAR_MAX);

	page(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW;

	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole page
	 * \param blocks count of blocks to be allocated in time
	 */
	BOOST_FORCEINLINE uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Releases previusly allocated memory if pointer is from this page
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of minimal allocated block
	 */
	BOOST_FORCEINLINE bool release(const uint8_t* ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;

    BOOST_FORCEINLINE bool is_empty() {
		return free_blocks_ == MAX_BLOCKS;
	}

private:
	uint8_t position_;
	uint8_t free_blocks_;
	const uint8_t* begin_;
	const uint8_t* end_;
	spin_lock mtx_;
};

/**
 * \brief Allocates only a signle memory block of the same size
 */
class fixed_allocator {
BOOST_MOVABLE_BUT_NOT_COPYABLE(fixed_allocator)
private:
	/// Vector of memory block pages
	typedef lockfree::forward_list<page*> pages_list;
	typedef boost::atomic<page*> local_ptr;
public:
	/// Constructs new fixed allocator of specific block size
	explicit fixed_allocator(const std::size_t block_size);
	/// releases allocator and all allocated memory
	~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
	/// Allocates a single memory block of fixed size
	inline void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(std::size_t size) BOOST_THROWS(std::bad_alloc);
	/**
	* Releases previesly allocated block of memory
	* \param ptr pointer to the allocated memory
	*/
	inline void free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr,const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
#ifdef BOOST_WINDOWS
	static void* operator new(const std::size_t size) BOOST_THROWS(std::bad_alloc)
	{
		return sys::xmalloc(size);
	}
	static void operator delete(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
	{
		sys::xfree(ptr);
	}
#endif // BOOST_WINDOWS
private:
	BOOST_FORCEINLINE page* create_new_page(std::size_t size) const;
	BOOST_FORCEINLINE void release_page(page* const pg) const BOOST_NOEXCEPT_OR_NOTHROW;
private:
	local_ptr alloc_current_;
	local_ptr free_current_;
	pages_list pages_;
};

class pool
{
BOOST_MOVABLE_BUT_NOT_COPYABLE(pool)
public:
	pool(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;
	~pool() BOOST_NOEXCEPT_OR_NOTHROW;
	BOOST_FORCEINLINE void *malloc BOOST_PREVENT_MACRO_SUBSTITUTION();
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	fixed_allocator* get_allocator();
private:
	boost::atomic<fixed_allocator*> allocator_;
	const std::size_t block_size_;
	static spin_lock _mtx;
};

/**
 * ! \brief Allocates memory for the small objects
 *  maximum size of small object is sizeof(size_type) * 16
 */
class object_allocator:private boost::noncopyable
{
private:
	typedef pool pool_t;
public:
	static const object_allocator* instance();
	BOOST_FORCEINLINE void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) const;
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW;
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	explicit object_allocator();
	void collect_free_memory() const;
	pool_t* get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW;
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static spin_lock _smtx;
	static boost::atomic<object_allocator*> _instance;
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
	void* operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc);
	void operator delete(void *ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	// support for boost::intrusive_ptr
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
