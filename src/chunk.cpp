#include "chunk.hpp"

namespace smallobject { namespace detail {

chunk::chunk(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW:
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

inline uint8_t* chunk::allocate(const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW {
	if(0 == free_blocks_) return NULL;
    uint8_t *result = const_cast<uint8_t*>( begin_ + (position_ * block_size) );
    position_ = *result;
    --free_blocks_;
	return result;
}

inline bool chunk::release(const uint8_t *ptr,const std::size_t block_size) BOOST_NOEXCEPT_OR_NOTHROW
{
    if( (ptr < begin_) || (ptr > end_) ) return false;
    *(const_cast<uint8_t*>(ptr)) = position_;
    position_ =  static_cast<uint8_t>( (ptr - begin_) / block_size );
    ++free_blocks_;
    return true;
}

} } // { namespace smallobject { namespace detail

