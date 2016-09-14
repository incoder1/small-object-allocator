#ifndef __SMALLOBJECT_POOL_HPP_INCLUDED__
#define __SMALLOBJECT_POOL_HPP_INCLUDED__

#include "arena.hpp"

#include "lockfreelist.hpp"
#include <boost/thread/tss.hpp>

namespace smallobject { namespace detail {

/// !\brief A pool of memory arenas for allocating
/// small object of fixed size memory
/// Reserves or creates one arena for each thread
class pool
{
public:
	explicit pool();
	~pool() BOOST_NOEXCEPT_OR_NOTHROW;
	BOOST_FORCEINLINE void *malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
	{
		if(NULL == arena_.get()) { reserve(size); }
		return arena_->malloc();
	}
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW
	{
	  if( ! arena_->free(ptr) ) {
			// handle allocation from another thread
			thread_miss_free(ptr);
	  }
	}
private:
	void thread_miss_free(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW;
	void reserve(const std::size_t size);
	static inline void release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	typedef smallobject::list<arena*> arenas_pool;
	boost::thread_specific_ptr<arena> arena_;
	arenas_pool arenas_;
};

}} //  namespace smallobject { namespace detail

#endif // __SMALLOBJECT_POOL_HPP_INCLUDED__
