#ifndef __SMALL_OBJECT_CHUNK_HPP_INCLUDED__
#define __SMALL_OBJECT_CHUNK_HPP_INCLUDED__

#include <climits>

#include <boost/config.hpp>
#include <boost/atomic.hpp>
#include <boost/cstdint.hpp>
#include <critical_section.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject { namespace detail {

/**
 * \brief A chunk of allocated memory divided on UCHAR_MAX of fixed blocks
 * Can only allocate UCHAR_MAX of fixed size blocks
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

	static const uint8_t MAX_BLOCKS;

	chunk(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW;

	#if !defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) && !defined(BOOST_NO_CXX11_NON_PUBLIC_DEFAULTED_FUNCTIONS)
	~chunk() = default;
	#endif

	/**
	 * Allocates memory blocks
	 * \param block_size size of minimal memory block, must be the same for the whole chunk
	 * \param blocks count of blocks to be allocated in time
	 */
	BOOST_FORCEINLINE uint8_t* allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
	{
		if (0 == free_blocks_) return NULL;
		uint8_t *result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
		position_ = *result;
		--free_blocks_;
		return result;
	}
	/**
	 * Releases previusly allocated memory if pointer is from this chunk
	 * \param ptr pointer on allocated memory
	 * \param bloc_size size of fixed allocated block
	 * \return true when memory has been freed, otherwise false
	 */
	BOOST_FORCEINLINE bool release(const uint8_t* ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
	{
		if( (ptr > end_) || (ptr < begin_) ) return false;
		*(const_cast<uint8_t*>(ptr)) = position_;
		position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
		++free_blocks_;
		return true;
	}

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
	const uint8_t* begin_;
	const uint8_t* end_;
	uint8_t free_blocks_;
	uint8_t position_;
};

} } // { namespace smallobject { namespace detail

#endif // __SMALL_OBJECT_CHUNK_HPP_INCLUDED__
