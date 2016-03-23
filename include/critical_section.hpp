#ifndef __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__
#define __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__

#include <boost/config.hpp>

#if  defined(BOOST_WINDOWS)
#	include "win/criticalsection.hpp"
#elif defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE)  || defined(_POSIX_SOURCE)
#	include "posix/spinlock.hpp"
#else // use boost mutex if no native spinlock
#	include "mutex_ciritical_section.hpp"
#endif // critical section select

#endif // __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__
