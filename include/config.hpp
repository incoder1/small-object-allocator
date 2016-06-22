#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif // BOOST_HAS_PRAGMA_ONCE

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
#if defined(BUILD_DLL) && defined(__GNUC__)
#	define SYMBOL_VISIBLE __attribute__((__dllexport__))
#elif defined(__GNUC__)
#	define SYMBOL_VISIBLE __attribute__((__dllimport__))
#elif defined(BUILD_DLL) && defined(_MSC_VER)
#	define SYMBOL_VISIBLE __declspec(dllexport)
#elif defined(_MSC_VER)
#	define SYMBOL_VISIBLE __declspec(dllimport)
#endif // BUILD_DLL
#else
#	define SYMBOL_VISIBLE BOOST_SYMBOL_VISIBLE
#endif // win32

#endif // CONFIG_HPP_INCLUDED
