//  (C) Copyright Dustin Spicuzza 2009.
//      Adapted to vxWorks 6.9 by Peter Brockamp 2012.
//      Updated for VxWorks 7 by Brian Kuhl 2016
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version.

//  Old versions of vxWorks (namely everything below 6.x) are
//  absolutely unable to use boost. Old STLs and compilers 
//  like (GCC 2.96) . Do not even think of getting this to work, 
//  a miserable failure will  be guaranteed!
//
//  VxWorks supports C++ linkage in the kernel with
//  DKMs (Downloadable Kernel Modules). But, until recently 
//  the kernel used a C89 library with no
//  wide character support and no guarantee of ANSI C. 
//  Regardless of the C library the same Dinkum 
//  STL library is used in both contexts. 
//
//  Similarly the Dinkum abridged STL that supports the loosely specified 
//  embedded C++ standard has not been tested and is unlikely to work 
//  on anything but the simplest library.
// ====================================================================
//
// Some important information regarding the usage of POSIX semaphores:
// -------------------------------------------------------------------
//
// VxWorks as a real time operating system handles threads somewhat
// different from what "normal" OSes do, regarding their scheduling!
// This could lead to a scenario called "priority inversion" when using
// semaphores, see http://en.wikipedia.org/wiki/Priority_inversion.
//
// Now, VxWorks POSIX-semaphores for DKM's default to the usage of
// priority inverting semaphores, which is fine. On the other hand,
// for RTP's it defaults to using non priority inverting semaphores,
// which could easily pose a serious problem for a real time process.
//
// To change the default properties for POSIX-semaphores in VxWorks 7
// enable core > CORE_USER Menu > DEFAULT_PTHREAD_PRIO_INHERIT 
//  
// In VxWorks 6.x so as to integrate with boost. 
// - Edit the file 
//   installDir/vxworks-6.x/target/usr/src/posix/pthreadLib.c
// - Around line 917 there should be the definition of the default
//   mutex attributes:
//
//   LOCAL pthread_mutexattr_t defaultMutexAttr =
//       {
//       PTHREAD_INITIALIZED_OBJ, PTHREAD_PRIO_NONE, 0,
//       PTHREAD_MUTEX_DEFAULT
//       };
//
//   Here, replace PTHREAD_PRIO_NONE by PTHREAD_PRIO_INHERIT.
// - Around line 1236 there should be a definition for the function
//   pthread_mutexattr_init(). A couple of lines below you should
//   find a block of code like this:
//
//   pAttr->mutexAttrStatus      = PTHREAD_INITIALIZED_OBJ;
//   pAttr->mutexAttrProtocol    = PTHREAD_PRIO_NONE;
//   pAttr->mutexAttrPrioceiling = 0;
//   pAttr->mutexAttrType        = PTHREAD_MUTEX_DEFAULT;
//
//   Here again, replace PTHREAD_PRIO_NONE by PTHREAD_PRIO_INHERIT.
// - Finally, rebuild your VSB. This will rebuild the libraries
//   with the changed properties. That's it! Now, using boost should
//   no longer cause any problems with task deadlocks!
//
//  ====================================================================

// Block out all versions before vxWorks 6.x, as these don't work:
// Include header with the vxWorks version information and query them
#include <version.h>
#if !defined(_WRS_VXWORKS_MAJOR) || (_WRS_VXWORKS_MAJOR < 6)
#  error "The vxWorks version you're using is so badly outdated,\
          it doesn't work at all with boost, sorry, no chance!"
#endif

// Handle versions above 5.X but below 6.9
#if (_WRS_VXWORKS_MAJOR == 6) && (_WRS_VXWORKS_MINOR < 9)
// TODO: Starting from what version does vxWorks work with boost?
// We can't reasonably insert a #warning "" as a user hint here,
// as this will show up with every file including some boost header,
// badly bugging the user... So for the time being we just leave it.
#endif

// vxWorks specific config options:
// --------------------------------
#define BOOST_PLATFORM "vxWorks"


// Generally available headers:
#define BOOST_HAS_UNISTD_H
#define BOOST_HAS_STDINT_H
#define BOOST_HAS_DIRENT_H
//#define BOOST_HAS_SLIST

// vxWorks does not have installed an iconv-library by default,
// so unfortunately no Unicode support from scratch is available!
// Thus, instead it is suggested to switch to ICU, as this seems
// to be the most complete and portable option...
#ifndef BOOST_LOCALE_WITH_ICU
   #define BOOST_LOCALE_WITH_ICU
#endif

// Generally available functionality:
#define BOOST_HAS_THREADS
#define BOOST_HAS_NANOSLEEP
#define BOOST_HAS_GETTIMEOFDAY
#define BOOST_HAS_CLOCK_GETTIME
#define BOOST_HAS_MACRO_USE_FACET

// Generally available threading API's:
#define BOOST_HAS_PTHREADS
#define BOOST_HAS_SCHED_YIELD
#define BOOST_HAS_SIGACTION

// Functionality available for RTPs only:
#ifdef __RTP__
#  define BOOST_HAS_PTHREAD_MUTEXATTR_SETTYPE
#  define BOOST_HAS_LOG1P
#  define BOOST_HAS_EXPM1
#endif

// Functionality available for DKMs only:
#ifdef _WRS_KERNEL
  // Luckily, at the moment there seems to be none!
#endif

// These #defines allow detail/posix_features to work, since vxWorks doesn't
// #define them itself for DKMs (for RTPs on the contrary it does):
#ifdef _WRS_KERNEL
#  ifndef _POSIX_TIMERS
#    define _POSIX_TIMERS  1
#  endif
#  ifndef _POSIX_THREADS
#    define _POSIX_THREADS 1
#  endif
// no sysconf( _SC_PAGESIZE) in kernel
#  define BOOST_THREAD_USES_GETPAGESIZE
#endif

#if (_WRS_VXWORKS_MAJOR < 7) 
// vxWorks-around: <time.h> #defines CLOCKS_PER_SEC as sysClkRateGet() but
//                 miserably fails to #include the required <sysLib.h> to make
//                 sysClkRateGet() available! So we manually include it here.
#  ifdef __RTP__
#    include <time.h>
#    include <sysLib.h>
#  endif

// vxWorks-around: In <stdint.h> the macros INT32_C(), UINT32_C(), INT64_C() and
//                 UINT64_C() are defined erroneously, yielding not a signed/
//                 unsigned long/long long type, but a signed/unsigned int/long
//                 type. Eventually this leads to compile errors in ratio_fwd.hpp,
//                 when trying to define several constants which do not fit into a
//                 long type! We correct them here by redefining.

#  include <cstdint>

// Special behaviour for DKMs:

// Some macro-magic to do the job
#  define VX_JOIN(X, Y)     VX_DO_JOIN(X, Y)
#  define VX_DO_JOIN(X, Y)  VX_DO_JOIN2(X, Y)
#  define VX_DO_JOIN2(X, Y) X##Y

// Correctly setup the macros
#  undef  INT32_C
#  undef  UINT32_C
#  undef  INT64_C
#  undef  UINT64_C
#  define INT32_C(x)  VX_JOIN(x, L)
#  define UINT32_C(x) VX_JOIN(x, UL)
#  define INT64_C(x)  VX_JOIN(x, LL)
#  define UINT64_C(x) VX_JOIN(x, ULL)

// #include Libraries required for the following function adaption
#  include <sys/time.h>
#endif  // _WRS_VXWORKS_MAJOR < 7

#include <ioLib.h>
#include <tickLib.h>

#if defined(_WRS_KERNEL) && (_CPPLIB_VER < 700)
  // recent kernels use Dinkum clib v7.00+
  // with widechar but older kernels
  // do not have the <cwchar>-header,
  // but apparently they do have an intrinsic wchar_t meanwhile!
#  define BOOST_NO_CWCHAR

  // Lots of wide-functions and -headers are unavailable for DKMs as well:
#  define BOOST_NO_CWCTYPE
#  define BOOST_NO_SWPRINTF
#  define BOOST_NO_STD_WSTRING
#  define BOOST_NO_STD_WSTREAMBUF
#endif


// Use C-linkage for the following helper functions
#ifdef __cplusplus
extern "C" {
#endif

// vxWorks-around: The required functions getrlimit() and getrlimit() are missing.
//                 But we have the similar functions getprlimit() and setprlimit(),
//                 which may serve the purpose.
//                 Problem: The vxWorks-documentation regarding these functions
//                 doesn't deserve its name! It isn't documented what the first two
//                 parameters idtype and id mean, so we must fall back to an educated
//                 guess - null, argh... :-/

// TODO: getprlimit() and setprlimit() do exist for RTPs only, for whatever reason.
//       Thus for DKMs there would have to be another implementation.
#if defined ( __RTP__) &&  (_WRS_VXWORKS_MAJOR < 7)
  inline int getrlimit(int resource, struct rlimit *rlp){
    return getprlimit(0, 0, resource, rlp);
  }

  inline int setrlimit(int resource, const struct rlimit *rlp){
    return setprlimit(0, 0, resource, const_cast<struct rlimit*>(rlp));
  }
#endif

// vxWorks has ftruncate() only, so we do simulate truncate():
inline int truncate(const char *p, off_t l){
  int fd = open(p, O_WRONLY);
  if (fd == -1){
    errno = EACCES;
    return -1;
  }
  if (ftruncate(fd, l) == -1){
    close(fd);
    errno = EACCES;
    return -1;
  }
  return close(fd);
}

#ifdef __GNUC__
#  define ___unused __attribute__((unused))
#else
#  define ___unused
#endif

// Fake symlink handling by dummy functions:
inline int symlink(const char* path1 ___unused, const char* path2 ___unused){
  // vxWorks has no symlinks -> always return an error!
  errno = EACCES;
  return -1;
}

inline ssize_t readlink(const char* path1 ___unused, char* path2 ___unused, size_t size ___unused){
  // vxWorks has no symlinks -> always return an error!
  errno = EACCES;
  return -1;
}

#if (_WRS_VXWORKS_MAJOR < 7)

inline int gettimeofday(struct timeval *tv, void * /*tzv*/) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  tv->tv_sec  = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
  return 0;
}
#endif

#ifdef __cplusplus
} // extern "C"
#endif

/* 
 * moved to os/utils/unix/freind_h/times.h in VxWorks 7
 * to avoid conflict with MPL operator times
 */
#if (_WRS_VXWORKS_MAJOR < 7) 
#  ifdef __cplusplus

// vxWorks provides neither struct tms nor function times()!
// We implement an empty dummy-function, simply setting the user
// and system time to the half of thew actual system ticks-value
// and the child user and system time to 0.
// Rather ugly but at least it suppresses compiler errors...
// Unfortunately, this of course *does* have an severe impact on
// dependant libraries, actually this is chrono only! Here it will
// not be possible to correctly use user and system times! But
// as vxWorks is lacking the ability to calculate user and system
// process times there seems to be no other possible solution.
struct tms{
  clock_t tms_utime;  // User CPU time
  clock_t tms_stime;  // System CPU time
  clock_t tms_cutime; // User CPU time of terminated child processes
  clock_t tms_cstime; // System CPU time of terminated child processes
};


 inline clock_t times(struct tms *t){
  struct timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  clock_t ticks(static_cast<clock_t>(static_cast<double>(ts.tv_sec)  * CLOCKS_PER_SEC +
                                     static_cast<double>(ts.tv_nsec) * CLOCKS_PER_SEC / 1000000.0));
  t->tms_utime  = ticks/2U;
  t->tms_stime  = ticks/2U;
  t->tms_cutime = 0; // vxWorks is lacking the concept of a child process!
  t->tms_cstime = 0; // -> Set the wait times for childs to 0
  return ticks;
}


namespace std {
    using ::times;
}
#  endif // __cplusplus
#endif // _WRS_VXWORKS_MAJOR < 7


#ifdef __cplusplus
extern "C" void   bzero     (void *, size_t);    // FD_ZERO uses bzero() but doesn't include strings.h

// Put the selfmade functions into the std-namespace, just in case
namespace std {
#  ifdef __RTP__
    using ::getrlimit;
    using ::setrlimit;
#  endif
  using ::truncate;
  using ::symlink;
  using ::readlink;
#  if (_WRS_VXWORKS_MAJOR < 7)  
    using ::gettimeofday;
#  endif  
}
#endif // __cplusplus

// Some more macro-magic:
// vxWorks-around: Some functions are not present or broken in vxWorks
//                 but may be patched to life via helper macros...

// Include signal.h which might contain a typo to be corrected here
#include <signal.h>

#if (_WRS_VXWORKS_MAJOR < 7)
#  define getpagesize()    sysconf(_SC_PAGESIZE)         // getpagesize is deprecated anyway!
inline int lstat(p, b) { return stat(p, b); }  // lstat() == stat(), as vxWorks has no symlinks!
#endif

#ifndef S_ISSOCK
#  define S_ISSOCK(mode) ((mode & S_IFMT) == S_IFSOCK) // Is file a socket?
#endif
#ifndef FPE_FLTINV
#  define FPE_FLTINV     (FPE_FLTSUB+1)                // vxWorks has no FPE_FLTINV, so define one as a dummy
#endif
#if !defined(BUS_ADRALN) && defined(BUS_ADRALNR)
#  define BUS_ADRALN     BUS_ADRALNR                   // Correct a supposed typo in vxWorks' <signal.h>
#endif
typedef int              locale_t;                     // locale_t is a POSIX-extension, currently not present in vxWorks!

// #include boilerplate code:
#include <boost/config/detail/posix_features.hpp>

// vxWorks lies about XSI conformance, there is no nl_types.h:
#undef BOOST_HAS_NL_TYPES_H

// vxWorks 7 adds C++11 support 
// however it is optional, and does not match exactly the support determined
// by examining the Dinkum STL version and GCC version (or ICC and DCC) 
#if !( defined( _WRS_CONFIG_LANG_LIB_CPLUS_CPLUS_USER_2011) || defined(_WRS_CONFIG_LIBCPLUS_STD))
#  define BOOST_NO_CXX11_ADDRESSOF      // C11 addressof operator on memory location
#  define BOOST_NO_CXX11_ALLOCATOR
#  define BOOST_NO_CXX11_ATOMIC_SMART_PTR
#  define BOOST_NO_CXX11_NUMERIC_LIMITS  // max_digits10 in test/../print_helper.hpp
#  define BOOST_NO_CXX11_SMART_PTR 
#  define BOOST_NO_CXX11_STD_ALIGN


#  define BOOST_NO_CXX11_HDR_ARRAY
#  define BOOST_NO_CXX11_HDR_ATOMIC
#  define BOOST_NO_CXX11_HDR_CHRONO
#  define BOOST_NO_CXX11_HDR_CONDITION_VARIABLE
#  define BOOST_NO_CXX11_HDR_FORWARD_LIST  //serialization/test/test_list.cpp
#  define BOOST_NO_CXX11_HDR_FUNCTIONAL 
#  define BOOST_NO_CXX11_HDR_FUTURE
#  define BOOST_NO_CXX11_HDR_MUTEX
#  define BOOST_NO_CXX11_HDR_RANDOM      //math/../test_data.hpp
#  define BOOST_NO_CXX11_HDR_RATIO
#  define BOOST_NO_CXX11_HDR_REGEX
#  define BOOST_NO_CXX14_HDR_SHARED_MUTEX
#  define BOOST_NO_CXX11_HDR_SYSTEM_ERROR
#  define BOOST_NO_CXX11_HDR_THREAD
#  define BOOST_NO_CXX11_HDR_TYPEINDEX 
#  define BOOST_NO_CXX11_HDR_TYPE_TRAITS
#  define BOOST_NO_CXX11_HDR_TUPLE 
#  define BOOST_NO_CXX11_HDR_UNORDERED_MAP
#  define BOOST_NO_CXX11_HDR_UNORDERED_SET 
#else
#  ifndef  BOOST_SYSTEM_NO_DEPRECATED
#    define BOOST_SYSTEM_NO_DEPRECATED  // workaround link error in spirit
#  endif
#endif


// NONE is used in enums in lamda and other libraries
#undef NONE
// restrict is an iostreams class
#undef restrict
// affects some typeof tests
#undef V7

// use fake poll() from Unix layer in ASIO to get full functionality 
// most libraries will use select() but this define allows 'iostream' functionality
// which is based on poll() only
#if (_WRS_VXWORKS_MAJOR > 6)
#  ifndef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
#    define BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
#  endif
#else 
#  define BOOST_ASIO_DISABLE_SERIAL_PORT
#endif

