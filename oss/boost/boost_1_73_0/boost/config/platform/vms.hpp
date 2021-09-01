//  (C) Copyright Artyom Beilis 2010.  
//  Use, modification and distribution are subject to the  
//  Boost Software License, Version 1.0. (See accompanying file  
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) 

#ifndef BOOST_CONFIG_PLATFORM_VMS_HPP 
#define BOOST_CONFIG_PLATFORM_VMS_HPP 

#define BOOST_PLATFORM "OpenVMS" 

#undef  BOOST_HAS_STDINT_H 
#define BOOST_HAS_UNISTD_H 
#define BOOST_HAS_NL_TYPES_H 
#define BOOST_HAS_GETTIMEOFDAY 
#define BOOST_HAS_DIRENT_H 
#define BOOST_HAS_PTHREADS 
#define BOOST_HAS_NANOSLEEP 
#define BOOST_HAS_CLOCK_GETTIME 
#define BOOST_HAS_PTHREAD_MUTEXATTR_SETTYPE 
#define BOOST_HAS_LOG1P 
#define BOOST_HAS_EXPM1 
#define BOOST_HAS_THREADS 
#undef  BOOST_HAS_SCHED_YIELD 

#endif 
