#ifndef __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__
#define __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__

#include <boost/config.hpp>

#if  defined(BOOST_WINDOWS)
#	include "win/criticalsection.hpp"
#elif defined(BOOST_POSIX_API)
#	include "posix/spinlock.hpp"
#else // use boost mutex if no native spinlock
#	include "mutex_ciritical_section.hpp"
#endif // critical section select

#endif // __SMALLOBJECT_WIN_CRITICAL_SECTION_HPP_INCLUDED__
