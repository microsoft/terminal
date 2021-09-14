//  boost/config/user.hpp  ---------------------------------------------------//

//  (C) Copyright John Maddock 2001. 
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  Do not check in modified versions of this file,
//  This file may be customized by the end user, but not by boost.

//
//  Use this file to define a site and compiler specific
//  configuration policy:
//

// define this to locate a compiler config file:
// #define BOOST_COMPILER_CONFIG <myheader>

// define this to locate a stdlib config file:
// #define BOOST_STDLIB_CONFIG   <myheader>

// define this to locate a platform config file:
// #define BOOST_PLATFORM_CONFIG <myheader>

// define this to disable compiler config,
// use if your compiler config has nothing to set:
// #define BOOST_NO_COMPILER_CONFIG

// define this to disable stdlib config,
// use if your stdlib config has nothing to set:
// #define BOOST_NO_STDLIB_CONFIG

// define this to disable platform config,
// use if your platform config has nothing to set:
// #define BOOST_NO_PLATFORM_CONFIG

// define this to disable all config options,
// excluding the user config.  Use if your
// setup is fully ISO compliant, and has no
// useful extensions, or for autoconf generated
// setups:
// #define BOOST_NO_CONFIG

// define this to make the config "optimistic"
// about unknown compiler versions.  Normally
// unknown compiler versions are assumed to have
// all the defects of the last known version, however
// setting this flag, causes the config to assume
// that unknown compiler versions are fully conformant
// with the standard:
// #define BOOST_STRICT_CONFIG

// define this to cause the config to halt compilation
// with an #error if it encounters anything unknown --
// either an unknown compiler version or an unknown
// compiler/platform/library:
// #define BOOST_ASSERT_CONFIG


// define if you want to disable threading support, even
// when available:
// #define BOOST_DISABLE_THREADS

// define when you want to disable Win32 specific features
// even when available:
// #define BOOST_DISABLE_WIN32

// BOOST_DISABLE_ABI_HEADERS: Stops boost headers from including any 
// prefix/suffix headers that normally control things like struct 
// packing and alignment. 
// #define BOOST_DISABLE_ABI_HEADERS

// BOOST_ABI_PREFIX: A prefix header to include in place of whatever
// boost.config would normally select, any replacement should set up 
// struct packing and alignment options as required. 
// #define BOOST_ABI_PREFIX my-header-name

// BOOST_ABI_SUFFIX: A suffix header to include in place of whatever 
// boost.config would normally select, any replacement should undo 
// the effects of the prefix header. 
// #define BOOST_ABI_SUFFIX my-header-name

// BOOST_ALL_DYN_LINK: Forces all libraries that have separate source, 
// to be linked as dll's rather than static libraries on Microsoft Windows 
// (this macro is used to turn on __declspec(dllimport) modifiers, so that 
// the compiler knows which symbols to look for in a dll rather than in a 
// static library).  Note that there may be some libraries that can only 
// be linked in one way (statically or dynamically), in these cases this 
// macro has no effect.
// #define BOOST_ALL_DYN_LINK
 
// BOOST_WHATEVER_DYN_LINK: Forces library "whatever" to be linked as a dll 
// rather than a static library on Microsoft Windows: replace the WHATEVER 
// part of the macro name with the name of the library that you want to 
// dynamically link to, for example use BOOST_DATE_TIME_DYN_LINK or 
// BOOST_REGEX_DYN_LINK etc (this macro is used to turn on __declspec(dllimport) 
// modifiers, so that the compiler knows which symbols to look for in a dll 
// rather than in a static library).  
// Note that there may be some libraries that can only 
// be linked in one way (statically or dynamically), 
// in these cases this macro is unsupported.
// #define BOOST_WHATEVER_DYN_LINK
 
// BOOST_ALL_NO_LIB: Tells the config system not to automatically select 
// which libraries to link against.  
// Normally if a compiler supports #pragma lib, then the correct library 
// build variant will be automatically selected and linked against, 
// simply by the act of including one of that library's headers.  
// This macro turns that feature off.
// #define BOOST_ALL_NO_LIB
 
// BOOST_WHATEVER_NO_LIB: Tells the config system not to automatically 
// select which library to link against for library "whatever", 
// replace WHATEVER in the macro name with the name of the library; 
// for example BOOST_DATE_TIME_NO_LIB or BOOST_REGEX_NO_LIB.  
// Normally if a compiler supports #pragma lib, then the correct library 
// build variant will be automatically selected and linked against, simply 
// by the act of including one of that library's headers.  This macro turns 
// that feature off.
// #define BOOST_WHATEVER_NO_LIB
 
// BOOST_LIB_BUILDID: Set to the same value as the value passed to Boost.Build's
// --buildid command line option.  For example if you built using:
//
// bjam address-model=64 --buildid=amd64
//
// then compile your code with:
//
// -DBOOST_LIB_BUILDID = amd64
//
// to ensure the correct libraries are selected at link time.
// #define BOOST_LIB_BUILDID amd64

