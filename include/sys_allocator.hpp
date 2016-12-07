#ifndef __SYS_ALOC_ONCE__
#define __SYS_ALOC_ONCE__

#include <boost/config.hpp>
// BOOST_NO_EXCEPTIONS
#include <boost/throw_exception.hpp>

#ifdef BOOST_WINDOWS
#	include "win/heapallocator.hpp"
#elif defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)
#	include "posix/xallocator.hpp"
#endif // defined

#include "critical_section.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject {

namespace sys {

template<typename T>
class allocator {
private:
	template<typename _Tp>
	static BOOST_CONSTEXPR inline _Tp* address_of(_Tp& __r) BOOST_NOEXCEPT_OR_NOTHROW
	{
		return reinterpret_cast<_Tp*>
		       (&const_cast<uint8_t&>(reinterpret_cast<const volatile uint8_t&>(__r)));
	}
public:
	typedef std::size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T&  reference;
	typedef const T& const_reference;
	typedef T value_type;

	template<typename T1>
	struct rebind {
		typedef sys::allocator<T1> other;
	};

	BOOST_CONSTEXPR allocator() BOOST_NOEXCEPT_OR_NOTHROW
	{ }

	BOOST_CONSTEXPR allocator(const allocator&) BOOST_NOEXCEPT_OR_NOTHROW
	{ }

	template<typename T1>
	BOOST_CONSTEXPR allocator(const allocator<T1>&) BOOST_NOEXCEPT_OR_NOTHROW
	{ }

	BOOST_FORCEINLINE ~allocator() BOOST_NOEXCEPT_OR_NOTHROW
	{}

	BOOST_CONSTEXPR inline pointer address(reference __x) const BOOST_NOEXCEPT
	{
		return address_of(__x);
	}

	BOOST_CONSTEXPR inline const_pointer address(const_reference __x) const BOOST_NOEXCEPT
	{
		return address_of<const_pointer>(__x);
	}

#ifndef BOOST_NO_EXCEPTIONS
	pointer allocate(size_type __n, const void* = 0)
	{
		void * result = xmalloc(__n * sizeof(value_type));
		if(NULL == result)
			throw std::bad_alloc();
		return static_cast<pointer>(result);
	}
#else
	pointer allocate(size_type __n, const void* = 0) BOOST_NOEXCEPT_OR_NOTHROW
	{
		pointer result = static_cast<pointer>( xmalloc(__n * sizeof(value_type)) );
#if (__cplusplus >= 201103L)
		if(nullptr == result) {
			std::new_handler h = std::get_new_handler();
			if(nullptr != h)
				h();
		}
#else
		// error no is alrady
		std::terminate();
#endif // __cplusplus
		return result;
	}
#endif // IO_NO_EXCEPTIONS

	// __p is not permitted to be a null pointer.
	void deallocate(pointer __p, size_type) BOOST_NOEXCEPT_OR_NOTHROW
	{
		assert(nullptr != __p);
		xfree(__p);
	}

#ifdef BOOST_HAS_VARIADIC_TMPL
	// addon to std::allocator make this noexcept(true) if constructing type is also noexcept
	template<typename _Up, typename... _Args>
	void construct(_Up* __p, _Args&&... __args) noexcept( noexcept( _Up(std::forward<_Args>(__args)...) ) )
	{
		assert(nullptr != __p);
		::new( static_cast<void *>(__p) ) _Up(std::forward<_Args>(__args)...);
	}
	template<typename _Up>
	void  destroy(_Up* __p) noexcept( noexcept( __p->~_Up() ) )
	{
		__p->~_Up();
	}
#endif // BOOST_HAS_VARIADIC_TMPL

	BOOST_FORCEINLINE size_type max_size() const noexcept
	{
		return SIZE_MAX / sizeof(value_type);
	}

};

template<typename _Tp>
BOOST_CONSTEXPR inline bool operator==(const allocator<_Tp>&, const allocator<_Tp>&)
{
	return true;
}

template<typename _Tp>
BOOST_CONSTEXPR inline bool operator!=(const allocator<_Tp>&, const allocator<_Tp>&)
{
	return false;
}

} // namespace sys

} // namespace smallobject

#endif // __SYS_ALOC_ONCE__
