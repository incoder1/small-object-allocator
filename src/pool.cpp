#include "pool.hpp"

namespace smallobject { namespace detail {

// poll
inline void pool::release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW
{
	ar->shrink();
	ar->release();
}

pool::pool() BOOST_NOEXCEPT_OR_NOTHROW:
	arena_(&pool::release_arena),
	mtx_()
{}

pool::~pool() BOOST_NOEXCEPT_OR_NOTHROW
{
	arenas_pool::iterator it= arenas_.begin();
	arenas_pool::iterator end = arenas_.end();
	while( it != end ) {
		arena* ar = *it;
		++it;
		delete ar;
	}
}

inline arena* pool::reserve(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW
{
	arena *result = arena_.get();
	if(!result) {
		boost::upgrade_lock<boost::shared_mutex> lock(this->mtx_);
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
			boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(lock);
			arenas_.push_back(result);

		}
		arena_.reset(result);
	}
	return result;
}

void* pool::malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
{
	arena *ar = reserve(size);
	return ar->malloc();
}

BOOST_FORCEINLINE void pool::free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
{
	bool released = arena_->free(ptr);
	// handle allocation from another thread
	while(!released) {
		boost::shared_lock<boost::shared_mutex> lock(mtx_);
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

