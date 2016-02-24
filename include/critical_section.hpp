#ifndef __CRITICAL_SECTION_HPP_INCLUDED__
#define __CRITICAL_SECTION_HPP_INCLUDED__

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#ifdef BOOST_WINDOWS_API
#	include "win/wincriticalsection.hpp"
#elif defined(BOOST_POSIX_API)
#endif // BOOST_WINDOWS_API


#endif // __CRITICAL_SECTION_HPP_INCLUDED__
