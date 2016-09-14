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

void pool::reserve(const std::size_t size)
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
		arenas_.push_front( arena_.get() );
	}
}


void pool::thread_miss_free(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW {
	arenas_pool::iterator it = arenas_.begin();
	arenas_pool::iterator end = arenas_.end();
	do {
		if( (*it)->synch_free(ptr) ) {
			return;
		}
	} while(++it != end);
}

}} //  namespace smallobject { namespace detail

