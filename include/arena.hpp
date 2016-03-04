#ifndef __SMALLOBJECT_ARENA_HPP_INCLUDED__
#define __SMALLOBJECT_ARENA_HPP_INCLUDED__

#include <boost/atomic.hpp>
#include <boost/throw_exception.hpp>

#include "chunk.hpp"
#include "noncopyable.hpp"
#include "critical_section.hpp"
#include "range_map.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace detail {

/// compares two pointers on allocated memory regions
struct byte_ptr_less : public std::binary_function<uint8_t*, uint8_t*, bool>
{
	BOOST_CONSTEXPR bool operator()(const uint8_t* lsh, const uint8_t* rhs) const {
		return lsh < rhs;
	}
};

/**
 * \brief Pool of reserved memory pages for allocating blocks of fixed size
 *  Memory allocation or releaseing is thread sefe, but only one thread must have a
 *  reserved arrena.
 *  Situation when one thread allocates memory, and another releases it bring to the
 *  perforamance bootleneck and should be avoided.
 *  Thread may reserve an arena released from an another thread in order to reuse
 *  allocated virtual memory
 */
class arena: public noncopyable {
BOOST_MOVABLE_BUT_NOT_COPYABLE(arena)
private:
	// a free list (free binary search AVL tree)
	typedef smallobject::synchronized_range_map<
			const uint8_t*,
			chunk*,
			byte_ptr_less,
			sys::allocator< movable_pair< const range<uint8_t*, byte_ptr_less >, chunk* > >
			> chunks_rmap;
	typedef chunks_rmap::range_type byte_ptr_range;
public:

	/// Constructs new arena of specific block size
	/// and allocates first chunk of reved virtual memory
	/// \param block_size size of fixed memory block in bytes
	explicit arena(const std::size_t block_size);

	/// Releases arena and all allocated virtual memory
	~arena() BOOST_NOEXCEPT_OR_NOTHROW;

	/// Allocates a single memory block of fixed size
	/// do system lock when thread releases memory allocated by this thread,
	/// or a thread previusly use this arena
	/// \return pointer on allocated memory block of fixed size
	/// \throw std::bad_alloc in case of system out of memory,
	/// calls user provided global boost::throw_exception in noexception mode
	inline void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION();

	/// Releases previesly allocated block of memory
	/// do system lock when thread releases memory allocated by another thread
	/// \param ptr pointer to the allocated memory
	/// \throw never trows
	inline bool free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) BOOST_NOEXCEPT_OR_NOTHROW;

	/// Makes attemp to reserve this arena for thread
	/// do spinlock
	/// \return true if success and false if arena alrady reserved by some thread
	/// \throw never thows
	bool reserve() BOOST_NOEXCEPT_OR_NOTHROW;

	/// Releases previusly reserved arena
	/// \throw never trows
	void release() BOOST_NOEXCEPT_OR_NOTHROW;

	/// Shinks no longer used memory, and returns it back to operating system
	/// may spinlocks and system locks
	void shrink() BOOST_NOEXCEPT_OR_NOTHROW;

private:
	/// Allocates system virtual memory pages for chunk
	/// do system lock
	/// \param size fixed chunk block size
	static BOOST_FORCEINLINE chunk* create_new_chunk(const std::size_t size);
	/// Releases system virtual memory back
	/// do system lock
	/// \param cnk pointer on memory chunk holder
	/// \param size size of fixed memory block
	static BOOST_FORCEINLINE void release_chunk(chunk* const cnk,const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	/// Makes attempt to allocate a memory block of fixed size from chunk
	/// do spinlock
	/// \return pointer on allocated memory block if success, otherwise NULL pointer
	/// \throw never throws
	BOOST_FORCEINLINE uint8_t* try_to_alloc(chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW;
	/// Makes attempt to release a memory block of fixed size from chunk
	/// do spinlock
	/// \return true whether sucesses, otherwise false
	BOOST_FORCEINLINE bool try_to_free(const uint8_t* ptr, chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW;
	/// Checks whether chunk is empty, do spinlock
	BOOST_FORCEINLINE bool is_empty_chunk(chunk* const chnk) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	const std::size_t block_size_;
	chunks_rmap chunks_;
	chunk* alloc_current_;
	chunk* free_current_;
	boost::atomic_flag reserved_;
	std::size_t free_chunks_;
	sys::critical_section mtx_;
};


}} // { namespace smallobject { namespace detail

#endif // __SMALLOBJECT_ARENA_HPP_INCLUDED__