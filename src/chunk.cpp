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

} } // { namespace smallobject { namespace detail

