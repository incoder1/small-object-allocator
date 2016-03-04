#ifndef __SMALL_OBJECT_HPP_INCLUDED__
#define __SMALL_OBJECT_HPP_INCLUDED__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#include "object_allocator.hpp"

namespace smallobject {

class object:public small_object {
protected:
	object();
private:
	// support for boost::intrusive_ptr
	boost::atomic_size_t ref_count_;
	friend void intrusive_ptr_add_ref(object* const obj);
	friend void intrusive_ptr_release(object* const obj);
};

} // namesapce smallobject

namespace boost {

using object = smallobject::object;

typedef boost::intrusive_ptr<object> s_object;

#ifndef BOOST_DECLARE_OBJECT_PTR_T
#	define BOOST_DECLARE_OBJECT_PTR_T(TYPE) typedef boost::intrusive_ptr<TYPE> s_##TYPE
#endif // DECLARE_SPTR_T

} // namespace boost

#endif // __LIBGC_OBJECT_HPP_INCLUDED__
