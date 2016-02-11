#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include <cstddef>
#include <new>
#include <list>

#include "nb_forward_list.hpp"

#include <boost/atomic.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/exception/exception.hpp>

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
	static const uint8_t MAX_BLOCKS;

	page(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW;

#if !defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) && !defined(BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS)
	~page() = default;
#else
	~page() BOOST_NOEXCEPT_OR_NOTHROW;
#endif

	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole page
	 * \param blocks count of blocks to be allocated in time
	 */
	inline uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Releases previusly allocated memory if pointer is from this page
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of minimal allocated block
	 */
	inline void release(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;

	BOOST_FORCEINLINE bool is_empty() {
		return free_blocks_ == MAX_BLOCKS;
	}

	BOOST_FORCEINLINE  bool in(const uint8_t *ptr) {
        return ptr >= begin_ && ptr < end_;
	}

private:
	uint8_t position_;
	uint8_t free_blocks_;
	const uint8_t* begin_;
	const uint8_t* end_;
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
	explicit fixed_allocator(const std::size_t block_size);
	/// releases allocator and all allocated memory
	~fixed_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
	/// Allocates a single memory block of fixed size
	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(std::size_t size) BOOST_THROWS(std::bad_alloc);
	/**
	* Releases previesly allocated block of memory
	* \param ptr pointer to the allocated memory
	*/
	bool free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr,const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Collect empty pages, i.e. collect garbage
	 */
	void collect() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	BOOST_FORCEINLINE page* create_new_page(std::size_t size) const;
	BOOST_FORCEINLINE void release_page(page* const pg) const BOOST_NOEXCEPT_OR_NOTHROW;
private:
	page* alloc_current_;
	page* free_current_;
	pages_list pages_;
	boost::mutex mtx_;
};

class pool
{
BOOST_MOVABLE_BUT_NOT_COPYABLE(pool)
private:
	typedef lockfree::forward_list<fixed_allocator*> fa_list;
public:
	explicit pool(const std::size_t block_size);
	~pool() BOOST_NOEXCEPT_OR_NOTHROW;
	void *malloc BOOST_PREVENT_MACRO_SUBSTITUTION();
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW;
	void collect_unlocked_memory() const BOOST_NOEXCEPT_OR_NOTHROW;
private:
	const std::size_t block_size_;
	fa_list allocators_;
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
	static object_allocator* const instance();
	void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size);
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	explicit object_allocator();
	void collect_free_memory() const;
	pool_t* get(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static boost::mutex _smtx;
	static boost::atomic<object_allocator*> volatile _instance;
	pool_t* pools_;
	//boost::thread gc_thread_;
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
