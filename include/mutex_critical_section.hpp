#ifndef MUTEX_CRITICAL_SECTION_HPP_INCLUDED
#define MUTEX_CRITICAL_SECTION_HPP_INCLUDED

#include <boost/thread/mutex.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

namespace smallobject {

namespace sys {
	typedef boost::mutex critical_section;
} // namespace sys


typedef boost::unique_lock<boost::mutex> unique_lock;

} //  namespace smallobject

#endif // MUTEX_CRITICAL_SECTION_HPP_INCLUDED
