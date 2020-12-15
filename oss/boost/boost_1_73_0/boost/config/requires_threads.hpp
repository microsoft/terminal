//  (C) Copyright John Maddock 2003. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_CONFIG_REQUIRES_THREADS_HPP
#define BOOST_CONFIG_REQUIRES_THREADS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_DISABLE_THREADS)

//
// special case to handle versions of gcc which don't currently support threads:
//
#if defined(__GNUC__) && ((__GNUC__ < 3) || (__GNUC_MINOR__ <= 3) || !defined(BOOST_STRICT_CONFIG))
//
// this is checked up to gcc 3.3:
//
#if defined(__sgi) || defined(__hpux)
#  error "Multi-threaded programs are not supported by gcc on HPUX or Irix (last checked with gcc 3.3)"
#endif

#endif

#  error "Threading support unavaliable: it has been explicitly disabled with BOOST_DISABLE_THREADS"

#elif !defined(BOOST_HAS_THREADS)

# if defined __COMO__
//  Comeau C++
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -D_MT (Windows) or -D_REENTRANT (Unix)"

#elif defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
//  Intel
#ifdef _WIN32
#  error "Compiler threading support is not turned on. Please set the correct command line options for threading: either /MT /MTd /MD or /MDd"
#else
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -openmp"
#endif

# elif defined __GNUC__
//  GNU C++:
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -pthread (Linux), -pthreads (Solaris) or -mthreads (Mingw32)"

#elif defined __sgi
//  SGI MIPSpro C++
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -D_SGI_MP_SOURCE"

#elif defined __DECCXX
//  Compaq Tru64 Unix cxx
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -pthread"

#elif defined __BORLANDC__
//  Borland
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -tWM"

#elif defined  __MWERKS__
//  Metrowerks CodeWarrior
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: either -runtime sm, -runtime smd, -runtime dm, or -runtime dmd"

#elif defined  __SUNPRO_CC
//  Sun Workshop Compiler C++
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -mt"

#elif defined __HP_aCC
//  HP aCC
#   error "Compiler threading support is not turned on. Please set the correct command line options for threading: -mt"

#elif defined(__IBMCPP__)
//  IBM Visual Age
#   error "Compiler threading support is not turned on. Please compile the code with the xlC_r compiler"

#elif defined _MSC_VER
//  Microsoft Visual C++
//
//  Must remain the last #elif since some other vendors (Metrowerks, for
//  example) also #define _MSC_VER
#  error "Compiler threading support is not turned on. Please set the correct command line options for threading: either /MT /MTd /MD or /MDd"

#else

#  error "Compiler threading support is not turned on.  Please consult your compiler's documentation for the appropriate options to use"

#endif // compilers

#endif // BOOST_HAS_THREADS

#endif // BOOST_CONFIG_REQUIRES_THREADS_HPP
