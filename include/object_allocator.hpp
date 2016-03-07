#ifndef __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
#define __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__

#include "pool.hpp"

#include <boost/intrusive_ptr.hpp>

namespace smallobject { namespace detail {

/**
 * ! \brief Allocates memory for the small objects
 *  maximum size of small object is sizeof(size_type) * 16
 */
class object_allocator:public smallobject::detail::noncopyable
{
public:
	// 64 for 32 bit and 128 for 64 bit instructions
	BOOST_STATIC_CONSTANT(std::size_t, MAX_SIZE = sizeof(std::size_t) * 16);
	static const object_allocator* instance();
	BOOST_FORCEINLINE void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) const
	{
		pool* p = get(size);
		assert(p);
		return p->malloc(size);
	}
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		pool* p = get(size);
		assert(p);
		p->free(ptr);
	}
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	explicit object_allocator();
	pool* get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW;
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static sys::critical_section _smtx;
	static boost::atomic<object_allocator*> _instance;
	pool* pools_;
};

} }  // namespace smallobject { namespace detail

#endif // __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
