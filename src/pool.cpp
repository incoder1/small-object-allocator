#include "pool.hpp"

namespace smallobject { namespace detail {

// poll
inline void pool::release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW
{
	ar->shrink();
	ar->release();
}

pool::pool():
	arena_(&pool::release_arena)
{}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
	// release arena allocated by current thread
	arena_.reset();
	// release all arenas
	arenas_pool::iterator it= arenas_.begin();
	arenas_pool::iterator end = arenas_.end();
	while( it != end ) {
		arena* ar = *it;
		++it;
		delete ar;
	}
}

void pool::reserve(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	arenas_pool::const_iterator it = arenas_.cbegin();
	arenas_pool::const_iterator end = arenas_.cend();
	while(it != end) {
		if( (*it)->reserve() ) {
			arena_.reset(*it);
			break;
		}
		++it;
	}
	if( NULL == arena_.get() ) {
		arena_.reset( new arena(size) );
		assert( arena_->reserve() );
		arenas_.push_front( arena_.get() );
	}
}


void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	bool released = arena_->free(ptr);
	// handle allocation from another thread
	while(!released) {
		arenas_pool::iterator it = arenas_.begin();
		arenas_pool::iterator end = arenas_.end();
		while(it != end) {
			assert(*it);
			released = (*it)->free(ptr);
			++it;
		}
	}
}

}} //  namespace smallobject { namespace detail

