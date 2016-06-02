#ifndef __SMALLOBJECT_NONCOPYABLE_HPP_INLUDED__
#define __SMALLOBJECT_NONCOPYABLE_HPP_INLUDED__

#include <boost/config.hpp>
#include <boost/noncopyable.hpp>

#include "sys_allocator.hpp"

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#ifndef BOOST_THROWS
#	ifdef BOOST_NO_CXX11_NOEXCEPT
#		define BOOST_THROWS(_EXC) throw(_EXC)
#	else
#		define BOOST_THROWS(_EXC)
#	endif  // BOOST_NO_CXX11_NOEXCEPT
#endif // BOOST_THROWS

namespace smallobject { namespace detail {

#if defined(_WIN32) || defined(_WIN64)
// windows specific noncopyable base to allocate arena struct data in same pages
// as allocation blocks to avoild CPU cache misses
class noncopyable: private boost::noncopyable
{
public:
	void* operator new(const std::size_t size) BOOST_THROWS(std::bad_alloc)
	{
        return smallobject::sys::xmalloc(size);
	}
	void operator delete(void* const ptr) BOOST_NOEXCEPT_OR_NOTHROW
	{
        smallobject::sys::xfree(ptr);
	}
};
#else
typedef boost::noncopyable noncopyable;
#endif // BOOST_WINDOWS

}} // namespace smallobject { namespace detail

#endif // __SMALLOBJECT_NONCOPYABLE_HPP_INLUDED__
