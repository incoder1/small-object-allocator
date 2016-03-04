#ifndef __SMALL_OBJECT_CHUNK_HPP_INCLUDED__
#define __SMALL_OBJECT_CHUNK_HPP_INCLUDED__

#include <climits>

#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace detail {

/**
 * ! \brief A chunk of allocated memory divided on UCHAR_MAX of fixed blocks
 */
class chunk
{
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
      chunk( const chunk& ) = delete;
      chunk& operator=( const chunk& ) = delete;
#else
  private:  // emphasize the following members are private
      chunk( const chunk& );
      chunk& operator=( const chunk& );
#endif // no deleted functions
public:
	BOOST_STATIC_CONSTANT(uint8_t, MAX_BLOCKS  = UCHAR_MAX);

	chunk(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW;

	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole chunk
	 * \param blocks count of blocks to be allocated in time
	 */
	inline uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;
	/**
	 * Releases previusly allocated memory if pointer is from this chunk
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of minimal allocated block
	 */
	inline bool release(const uint8_t* ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW;

	BOOST_FORCEINLINE bool empty() BOOST_NOEXCEPT_OR_NOTHROW
	{
		return MAX_BLOCKS == free_blocks_;
	}

	BOOST_FORCEINLINE const uint8_t* begin() {
		return begin_;
	}

	BOOST_FORCEINLINE const uint8_t* end() {
		return end_;
	}

private:
	uint8_t position_;
	uint8_t free_blocks_;
	const uint8_t* begin_;
	const uint8_t* end_;
};

} } // { namespace smallobject { namespace detail

#endif // __SMALL_OBJECT_CHUNK_HPP_INCLUDED__
