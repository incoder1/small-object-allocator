#include "arena.hpp"

namespace smallobject { namespace detail {

//arena
BOOST_FORCEINLINE chunk* arena::create_new_chunk(const std::size_t size)
{
	void *ptr = sys::xmalloc( sizeof(chunk) + (size * chunk::MAX_BLOCKS) );
	const uint8_t *begin = static_cast<uint8_t*>(ptr) + sizeof(chunk);
	return new (ptr) chunk(size, begin);
}

BOOST_FORCEINLINE void arena::release_chunk(chunk* const cnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	// assert(cnk);
	cnk->~chunk();
	sys::xfree( static_cast<void*>(cnk) );
}

arena::arena(const std::size_t block_size):
	block_size_(block_size),
	chunks_(),
	alloc_current_(NULL),
	free_current_(NULL),
	reserved_(),
	rwb_()
{
	reserved_.test_and_set();
	chunk* first = create_new_chunk( block_size_ );
	alloc_current_ = first;
	free_current_  = first;
	chunks_.insert( first->begin(),  first->end(), BOOST_MOVE_BASE(chunk*,first) );
}

arena::~arena() BOOST_NOEXCEPT_OR_NOTHROW
{
	for(chunks_rmap::iterator it= chunks_.begin(); it != chunks_.end(); ++it) {
		release_chunk( it->second );
	}
}

BOOST_FORCEINLINE uint8_t* arena::try_to_alloc(chunk* const chnk) BOOST_NOEXCEPT_OR_NOTHROW
{
	sys::read_lock lock(rwb_);
	uint8_t *result = chnk->allocate(block_size_);
	if(NULL != result) {
		alloc_current_ = chnk;
	}
	return result;
}

void* arena::malloc BOOST_PREVENT_MACRO_SUBSTITUTION() BOOST_THROWS(std::bad_alloc)
{
	uint8_t* result = try_to_alloc(alloc_current_);
	if(NULL != result) return static_cast<void*>(result);
	// search in reserved space
	chunk* current = NULL;
	chunks_rmap::iterator it = chunks_.begin();
	chunks_rmap::iterator end = chunks_.end();
	while( it != end ) {
		current = it->second;
		result = try_to_alloc(current);
		if(result) {
			return static_cast<void*>(result);
		}
		++it;
	}
	// no free space left, create new chunk
	current = create_new_chunk(block_size_);
	result = current->allocate(block_size_);
	sys::write_lock lock(rwb_);
	chunks_.insert(current->begin(), current->end(), BOOST_MOVE_BASE(chunk*,current) );
	alloc_current_ = current;
	free_current_ = current;
	return static_cast<void*>(result);
}

bool arena::lookup_chunk_and_free(const uint8_t *ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	chunks_rmap::iterator it = chunks_.find(ptr);
	if(it == chunks_.end() ) return false;
	try_to_free(ptr, it->second);
	return true;
}

void arena::shrink() BOOST_NOEXCEPT_OR_NOTHROW {
	sys::write_lock lock(rwb_);
	typedef std::vector<chunk*, sys::allocator<chunk*> > chvector;
	chvector non_empty;
	chunks_rmap::iterator it = chunks_.begin();
	chunks_rmap::iterator end = chunks_.end();
	while(it != end)
	{
		if(!it->second->empty()) {
#if __cplusplus >= 201103L
			non_empty.emplace_back( it->second );
#else
			non_empty.push_back( BOOST_MOVE_BASE(chunk*, it->second) );
#endif // __cplusplus
		} else {
			release_chunk( it->second );
		}
		++it;
	}
	chunks_.clear();
	chvector::iterator i = non_empty.begin();
	chvector::iterator last = non_empty.end();
	while(i != last) {
		chunks_.insert( (*i)->begin(), (*i)->end(), BOOST_MOVE_BASE(chunk*,*i) );
		++i;
	}
	non_empty.clear();
	if(chunks_.empty()) {
		chunk* first = create_new_chunk( block_size_ );
		alloc_current_ = first;
		free_current_  = first;
		chunks_.insert( first->begin(),  first->end(), BOOST_MOVE_BASE(chunk*,first) );
	} else {
		alloc_current_ = chunks_.begin()->second;
		free_current_ = alloc_current_;
	}
}

}} //  namespace smallobject { namespace detail
