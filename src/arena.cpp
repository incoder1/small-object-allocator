#include "arena.hpp"

namespace smallobject { namespace detail {

//arena
BOOST_FORCEINLINE chunk* arena::create_new_chunk(const std::size_t size)
{
	void *ptr = sys::xmalloc( sizeof(chunk) + (size * chunk::MAX_BLOCKS) );
	const uint8_t *begin = static_cast<uint8_t*>(ptr) + sizeof(chunk);
	return new (ptr) chunk(size, begin);
}

BOOST_FORCEINLINE void arena::release_chunk(chunk* const cnk, const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	assert(cnk);
	cnk->~chunk();
	sys::xfree( static_cast<void*>(cnk), size + sizeof(chunk) );
}

arena::arena(const std::size_t block_size):
	block_size_(block_size),
	chunks_(),
	alloc_current_(NULL),
	free_current_(NULL),
	free_chunks_(0)
{
	chunk* first = create_new_chunk( block_size_ );
	alloc_current_ = first;
	free_current_  = first;
	chunks_.insert( first->begin(),  first->end(), BOOST_MOVE_BASE(chunk*,first) );
}

arena::~arena() BOOST_NOEXCEPT_OR_NOTHROW
{
	for(chunks_rmap::iterator it= chunks_.begin(); it != chunks_.end(); ++it) {
		release_chunk( it->second , block_size_ );
	}
}

BOOST_FORCEINLINE uint8_t* arena::try_to_alloc(chunk* const chnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	unique_lock lock(mtx_);
    return chnk->allocate(block_size_);
}

void* arena::malloc BOOST_PREVENT_MACRO_SUBSTITUTION() BOOST_THROWS(std::bad_alloc)
{
	chunk* current = alloc_current_;
	uint8_t* result = try_to_alloc(current);
	if(NULL == result) {
		chunks_rmap::iterator it = chunks_.begin();
		chunks_rmap::iterator end = chunks_.end();
		while( it != end ) {
			current = it->second;
			result = try_to_alloc(current);
			if(result) {
				alloc_current_ = current;
				break;
			}
			++it;
		}
	}
	// no space left, create new chunk
	if(NULL == result) {
		current = create_new_chunk(block_size_);
		result = current->allocate(block_size_);
		chunks_.insert(current->begin(), current->end(), BOOST_MOVE_BASE(chunk*,current) );
		alloc_current_ = current;
		free_current_ = current;
	}
	return static_cast<void*>(result);
}


bool arena::reserve() BOOST_NOEXCEPT_OR_NOTHROW
{
	return ! reserved_.test_and_set(boost::memory_order_acquire);
}

void arena::release() BOOST_NOEXCEPT_OR_NOTHROW
{
	reserved_.clear(boost::memory_order_release);
}

inline bool arena::try_to_free(const uint8_t* ptr, chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	unique_lock lock(mtx_);
	bool result = cnk->release(ptr,block_size_);
	if(result) {
		free_current_ = cnk;
		if( cnk->empty() ) {
            ++free_chunks_;
		}
	}
	return result;
}

bool arena::free BOOST_PREVENT_MACRO_SUBSTITUTION(void const *ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	const uint8_t *p = static_cast<const uint8_t*>(ptr);
	chunk* current = free_current_;
	if( !try_to_free(p, current) ) {
		chunks_rmap::iterator it = chunks_.find(p);
		if( it != chunks_.end() ) {
			current = it->second;
			assert( try_to_free(p,current) );
			free_current_ = current;
		} else {
			return false;
		}
		if(free_chunks_ > 2) {
			shrink();
		}
	}
	return true;
}

BOOST_FORCEINLINE bool arena::is_empty_chunk(chunk* const chnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	unique_lock lock(mtx_);
	return chnk->empty();
}

void arena::shrink() BOOST_NOEXCEPT_OR_NOTHROW {
	chunks_rmap::iterator it = chunks_.begin();
	chunks_rmap::iterator end = chunks_.end();
	free_chunks_ = 0;
	while(it != end)
	{
		if( is_empty_chunk(it->second) ) {
			if(++free_chunks_ > 2) {
				chunks_rmap::iterator rit = it;
				++it;
				chunks_.erase(rit);
			} else {
				++it;
			}
		} else {
			++it;
		}
	}
	free_chunks_ = 2;
}

}} //  namespace smallobject { namespace detail