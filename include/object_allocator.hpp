#ifndef __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
#define __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__

#include "pool.hpp"

#include <boost/intrusive_ptr.hpp>

namespace smallobject {

namespace detail {

/**
 * ! \brief Allocates memory for the small objects
 *  maximum size of small object is sizeof(size_type) * 16
 */
class object_allocator:public noncopyable
{
private:
	typedef pool pool_t;
public:
	static const object_allocator* instance();
	BOOST_FORCEINLINE void* malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const std::size_t size) const;
	BOOST_FORCEINLINE void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *ptr, const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW;
	~object_allocator() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	explicit object_allocator();
	BOOST_FORCEINLINE pool_t* get(const std::size_t size) const BOOST_NOEXCEPT_OR_NOTHROW;
	static void release() BOOST_NOEXCEPT_OR_NOTHROW;
private:
	static sys::critical_section _smtx;
	static boost::atomic<object_allocator*> _instance;
	pool_t* pools_;
};

} // namespace detail

class small_object
{
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
	small_object( const small_object& ) = delete;
	small_object& operator=( const  small_object& ) = delete;
#else
  private:  // emphasize the following members are private
      basic_object( const basic_object& );
      basic_object& operator=( const basic_object& );
#endif // no deleted functions
protected:
	object() BOOST_NOEXCEPT_OR_NOTHROW;
public:
	virtual ~object() BOOST_NOEXCEPT_OR_NOTHROW = 0;
	// redefine new and delete operations for small object optimized memory allocation
	void* operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc);
	void operator delete(void *ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW;
};


} // namesapce smallobject

#endif // __SMALLOBJECT_OBJECT_ALLOCATOR_HPP_INCLUDED__
