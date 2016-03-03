#ifndef __BOOST_SMALLOBJECT_CRITICAL_SECTION_HPP_INCLUDED__
#define __BOOST_SMALLOBJECT_CRITICAL_SECTION_HPP_INCLUDED__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#ifdef BOOST_WINDOWS_API
#	include "win/criticalsection.hpp"
#elif defined(BOOST_POSIX_API)
#	include "posix/spinlock.hpp"
#else // use boost mutex if no native critical sections

#include <boost/thread/mutex.hpp>

namespace boost { namespace smallobject { namespace sys {

using critical_section = boost::mutex;

}}} // namespace boost { namespace smallobject { namespace sys

#endif // BOOST_WINDOWS_API

#endif // __SMALLOBJECT_POSIX_CRITICAL_SECTION_HPP_INCLUDED__
