#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include "config.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#include "object_allocator.hpp"

namespace smallobject {

class SYMBOL_VISIBLE object
{
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
	object( const object& ) = delete;
	object& operator=( const object& ) = delete;
#else
  private:  // emphasize the following members are private
      object( const object& );
      object& operator=( const object& );
#endif // no deleted functions
protected:
	BOOST_CONSTEXPR object() BOOST_NOEXCEPT_OR_NOTHROW:
		ref_count_(0)
	{}
public:
	virtual ~object() BOOST_NOEXCEPT_OR_NOTHROW = 0;
	// redefine new and delete operations for small object optimized memory allocation
	void* operator new(std::size_t bytes) BOOST_THROWS(std::bad_alloc);
	void operator delete(void *ptr,std::size_t bytes) BOOST_NOEXCEPT_OR_NOTHROW;
private:
	boost::atomic_size_t ref_count_;
	friend BOOST_FORCEINLINE void intrusive_ptr_add_ref(object* const obj) noexcept
	{
		obj->ref_count_.fetch_add(1, boost::memory_order_relaxed);
	}
	friend BOOST_FORCEINLINE void intrusive_ptr_release(object* const obj) noexcept
	{
		if (obj->ref_count_.fetch_sub(1, boost::memory_order_release) == 1) {
			boost::atomic_thread_fence(boost::memory_order_acquire);
			delete obj;
		}
	}
};

typedef boost::intrusive_ptr<object> s_object;

} // namesapce smallobject

#ifndef DECLARE_OBJECT_PTR_T
#	define DECLARE_OBJECT_PTR_T(TYPE) typedef boost::intrusive_ptr<TYPE> s_##TYPE
#endif // DECLARE_OBJECT_PTR_T

#endif // __LIBGC_OBJECT_HPP_INCLUDED__
