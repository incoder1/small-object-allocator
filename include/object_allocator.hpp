#ifndef __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
#define __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__

#include "pool.hpp"

#include <boost/intrusive_ptr.hpp>

namespace smallobject { namespace detail {

extern const std::size_t SHIFT;

BOOST_CONSTEXPR BOOST_FORCEINLINE std::size_t align_up(const std::size_t alignment,const std::size_t size) BOOST_NOEXCEPT
{
	return ( size + (alignment - 1) ) & ~(alignment - 1);
}

/**
 * ! \brief Allocates memory for the small objects
 *  maximum size of small object is sizeof(size_type) * 16
 */
class object_allocator:public smallobject::detail::noncopyable
{
public:
	// 64 for 32 bit and 128 for 64 bit instructions
	static const std::size_t MAX_SIZE;
private:
	// 8 bytes for 32 bit and 16 bytes for 64 bit
	static const std::size_t MIN_SIZE;
	// 2 since object size not changed
	static const std::size_t SHIFT;
	// 14 pools
	static const std::size_t POOLS_COUNT;
public:
	static object_allocator* instance();
	BOOST_FORCEINLINE void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size)
	{
		return get(size)->malloc(size);
	}
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		get(size)->free(ptr);
	}
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	explicit object_allocator();
	BOOST_FORCEINLINE pool* get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW
	{
		std::size_t index = ( align_up(sizeof(std::size_t),size) / sizeof(std::size_t) ) - SHIFT;
		return pools_ + index;
	}
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static sys::critical_section _smtx;
	static boost::atomic<object_allocator*> _instance;
	pool* pools_;
};

} }  // namespace smallobject { namespace detail

#endif // __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
