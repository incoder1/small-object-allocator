#ifndef __SMALLOBJECT_POOL_HPP_INCLUDED__
#define __SMALLOBJECT_POOL_HPP_INCLUDED__

#include "arena.hpp"

#include "lockfreelist.hpp"
#include <boost/thread/tss.hpp>

namespace smallobject { namespace detail {

class pool
{
BOOST_MOVABLE_BUT_NOT_COPYABLE(pool)
public:
	explicit pool();
	~pool() BOOST_NOEXCEPT_OR_NOTHROW;
	void *malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
	{
		if(NULL == arena_.get()) {
			reserve(size);
		}
		return arena_->malloc();
	}
	void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const ptr) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	void reserve(const std::size_t size) BOOST_NOEXCEPT_OR_NOTHROW;
	static inline void release_arena(arena* ar) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	typedef smallobject::list<arena*> arenas_pool;
	boost::thread_specific_ptr<arena> arena_;
	arenas_pool arenas_;
};

}} //  namespace smallobject { namespace detail

#endif // __SMALLOBJECT_POOL_HPP_INCLUDED__
