#include "pool.hpp"

namespace smallobject { namespace detail {

// poll
inline void pool::release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW
{
	ar->shrink();
	ar->release();
}

pool::pool():
	arena_(&pool::release_arena),
	mtx_()
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

arena* pool::reserve(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	arena *result = arena_.get();
	if(!result) {
		arenas_pool::iterator it = arenas_.begin();
		arenas_pool::iterator end = arenas_.end();
		while(it != end) {
            bool reserved = (*it)->reserve();
			if(reserved) {
				result = *it;
				break;
			}
			++it;
		}
		if(!result) {
			result = new arena(size);
			arenas_.push_front( BOOST_MOVE_BASE(arena*, result) );
		}
		arena_.reset(result);
	}
	return result;
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

