#include "chunk.hpp"

namespace smallobject { namespace detail {

const uint8_t chunk::MAX_BLOCKS = UCHAR_MAX;

chunk::chunk(const uint8_t block_size, const uint8_t* begin) BOOST_NOEXCEPT_OR_NOTHROW:
	begin_( begin ),
	end_(begin + (MAX_BLOCKS * block_size) ),
	free_blocks_(MAX_BLOCKS),
	position_(0)
{
	uint8_t* p = const_cast<uint8_t*>(begin_);
	for(uint8_t i=1; i < MAX_BLOCKS; i++) {
		*p = i;
		p += block_size;
	}
}

} } // { namespace smallobject { namespace detail

