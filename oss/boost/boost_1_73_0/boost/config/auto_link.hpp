//  (C) Copyright John Maddock 2003.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         auto_link.hpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Automatic library inclusion for Borland/Microsoft compilers.
  */

/*************************************************************************

USAGE:
~~~~~~

Before including this header you must define one or more of define the following macros:

BOOST_LIB_NAME:           Required: A string containing the basename of the library,
                          for example boost_regex.
BOOST_LIB_TOOLSET:        Optional: the base name of the toolset.
BOOST_DYN_LINK:           Optional: when set link to dll rather than static library.
BOOST_LIB_DIAGNOSTIC:     Optional: when set the header will print out the name
                          of the library selected (useful for debugging).
BOOST_AUTO_LINK_NOMANGLE: Specifies that we should link to BOOST_LIB_NAME.lib,
                          rather than a mangled-name version.
BOOST_AUTO_LINK_TAGGED:   Specifies that we link to libraries built with the --layout=tagged option.
                          This is essentially the same as the default name-mangled version, but without
                          the compiler name and version, or the Boost version.  Just the build options.
BOOST_AUTO_LINK_SYSTEM:   Specifies that we link to libraries built with the --layout=system option.
                          This is essentially the same as the non-name-mangled version, but with
                          the prefix to differentiate static and dll builds

These macros will be undef'ed at the end of the header, further this header
has no include guards - so be sure to include it only once from your library!

Algorithm:
~~~~~~~~~~

Libraries for Borland and Microsoft compilers are automatically
selected here, the name of the lib is selected according to the following
formula:

BOOST_LIB_PREFIX
   + BOOST_LIB_NAME
   + "_"
   + BOOST_LIB_TOOLSET
   + BOOST_LIB_THREAD_OPT
   + BOOST_LIB_RT_OPT
   + BOOST_LIB_ARCH_AND_MODEL_OPT
   "-"
   + BOOST_LIB_VERSION

These are defined as:

BOOST_LIB_PREFIX:     "lib" for static libraries otherwise "".

BOOST_LIB_NAME:       The base name of the lib ( for example boost_regex).

BOOST_LIB_TOOLSET:    The compiler toolset name (vc6, vc7, bcb5 etc).

BOOST_LIB_THREAD_OPT: "-mt" for multithread builds, otherwise nothing.

BOOST_LIB_RT_OPT:     A suffix that indicates the runtime library used,
                      contains one or more of the following letters after
                      a hyphen:

                      s      static runtime (dynamic if not present).
                      g      debug/diagnostic runtime (release if not present).
                      y      Python debug/diagnostic runtime (release if not present).
                      d      debug build (release if not present).
                      p      STLport build.
                      n      STLport build without its IOStreams.

BOOST_LIB_ARCH_AND_MODEL_OPT: The architecture and address model
                              (-x32 or -x64 for x86/32 and x86/64 respectively)

BOOST_LIB_VERSION:    The Boost version, in the form x_y, for Boost version x.y.


***************************************************************************/

#ifdef __cplusplus
#  ifndef BOOST_CONFIG_HPP
#     include <boost/config.hpp>
#  endif
#elif defined(_MSC_VER) && !defined(__MWERKS__) && !defined(__EDG_VERSION__)
//
// C language compatability (no, honestly)
//
#  define BOOST_MSVC _MSC_VER
#  define BOOST_STRINGIZE(X) BOOST_DO_STRINGIZE(X)
#  define BOOST_DO_STRINGIZE(X) #X
#endif
//
// Only include what follows for known and supported compilers:
//
#if defined(BOOST_MSVC) \
    || defined(__BORLANDC__) \
    || (defined(__MWERKS__) && defined(_WIN32) && (__MWERKS__ >= 0x3000)) \
    || (defined(__ICL) && defined(_MSC_EXTENSIONS) && (_MSC_VER >= 1200)) \
    || (defined(BOOST_CLANG) && defined(BOOST_WINDOWS) && defined(_MSC_VER) && (__clang_major__ >= 4))

#ifndef BOOST_VERSION_HPP
#  include <boost/version.hpp>
#endif

#ifndef BOOST_LIB_NAME
#  error "Macro BOOST_LIB_NAME not set (internal error)"
#endif

//
// error check:
//
#if defined(__MSVC_RUNTIME_CHECKS) && !defined(_DEBUG)
#  pragma message("Using the /RTC option without specifying a debug runtime will lead to linker errors")
#  pragma message("Hint: go to the code generation options and switch to one of the debugging runtimes")
#  error "Incompatible build options"
#endif
//
// select toolset if not defined already:
//
#ifndef BOOST_LIB_TOOLSET
#  if defined(BOOST_MSVC) && (BOOST_MSVC < 1200)
    // Note: no compilers before 1200 are supported
#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1300)

#    ifdef UNDER_CE
       // eVC4:
#      define BOOST_LIB_TOOLSET "evc4"
#    else
       // vc6:
#      define BOOST_LIB_TOOLSET "vc6"
#    endif

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1310)

     // vc7:
#    define BOOST_LIB_TOOLSET "vc7"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1400)

     // vc71:
#    define BOOST_LIB_TOOLSET "vc71"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1500)

     // vc80:
#    define BOOST_LIB_TOOLSET "vc80"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1600)

     // vc90:
#    define BOOST_LIB_TOOLSET "vc90"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1700)

     // vc10:
#    define BOOST_LIB_TOOLSET "vc100"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1800)

     // vc11:
#    define BOOST_LIB_TOOLSET "vc110"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1900)

     // vc12:
#    define BOOST_LIB_TOOLSET "vc120"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1910)

     // vc14:
#    define BOOST_LIB_TOOLSET "vc140"

#  elif defined(BOOST_MSVC) && (BOOST_MSVC < 1920)

     // vc14.1:
#    define BOOST_LIB_TOOLSET "vc141"

#  elif defined(BOOST_MSVC)

     // vc14.2:
#    define BOOST_LIB_TOOLSET "vc142"

#  elif defined(__BORLANDC__)

     // CBuilder 6:
#    define BOOST_LIB_TOOLSET "bcb"

#  elif defined(__ICL)

     // Intel C++, no version number:
#    define BOOST_LIB_TOOLSET "iw"

#  elif defined(__MWERKS__) && (__MWERKS__ <= 0x31FF )

     // Metrowerks CodeWarrior 8.x
#    define BOOST_LIB_TOOLSET "cw8"

#  elif defined(__MWERKS__) && (__MWERKS__ <= 0x32FF )

     // Metrowerks CodeWarrior 9.x
#    define BOOST_LIB_TOOLSET "cw9"

#  elif defined(BOOST_CLANG) && defined(BOOST_WINDOWS) && defined(_MSC_VER) && (__clang_major__ >= 4)

     // Clang on Windows
#    define BOOST_LIB_TOOLSET "clangw" BOOST_STRINGIZE(__clang_major__)

#  endif
#endif // BOOST_LIB_TOOLSET

//
// select thread opt:
//
#if defined(_MT) || defined(__MT__)
#  define BOOST_LIB_THREAD_OPT "-mt"
#else
#  define BOOST_LIB_THREAD_OPT
#endif

#if defined(_MSC_VER) || defined(__MWERKS__)

#  ifdef _DLL

#     if (defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)) && (defined(_STLP_OWN_IOSTREAMS) || defined(__STL_OWN_IOSTREAMS))

#        if defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-gydp"
#        elif defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))
#            define BOOST_LIB_RT_OPT "-gdp"
#        elif defined(_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-gydp"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        elif defined(_DEBUG)
#            define BOOST_LIB_RT_OPT "-gdp"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        else
#            define BOOST_LIB_RT_OPT "-p"
#        endif

#     elif defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)

#        if defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-gydpn"
#        elif defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))
#            define BOOST_LIB_RT_OPT "-gdpn"
#        elif defined(_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-gydpn"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        elif defined(_DEBUG)
#            define BOOST_LIB_RT_OPT "-gdpn"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        else
#            define BOOST_LIB_RT_OPT "-pn"
#        endif

#     else

#        if defined(_DEBUG) && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-gyd"
#        elif defined(_DEBUG)
#            define BOOST_LIB_RT_OPT "-gd"
#        else
#            define BOOST_LIB_RT_OPT
#        endif

#     endif

#  else

#     if (defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)) && (defined(_STLP_OWN_IOSTREAMS) || defined(__STL_OWN_IOSTREAMS))

#        if defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-sgydp"
#        elif defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))
#            define BOOST_LIB_RT_OPT "-sgdp"
#        elif defined(_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#             define BOOST_LIB_RT_OPT "-sgydp"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        elif defined(_DEBUG)
#             define BOOST_LIB_RT_OPT "-sgdp"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        else
#            define BOOST_LIB_RT_OPT "-sp"
#        endif

#     elif defined(__SGI_STL_PORT) || defined(_STLPORT_VERSION)

#        if defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#            define BOOST_LIB_RT_OPT "-sgydpn"
#        elif defined(_DEBUG) && (defined(__STL_DEBUG) || defined(_STLP_DEBUG))
#            define BOOST_LIB_RT_OPT "-sgdpn"
#        elif defined(_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#             define BOOST_LIB_RT_OPT "-sgydpn"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        elif defined(_DEBUG)
#             define BOOST_LIB_RT_OPT "-sgdpn"
#            pragma message("warning: STLport debug versions are built with /D_STLP_DEBUG=1")
#            error "Build options aren't compatible with pre-built libraries"
#        else
#            define BOOST_LIB_RT_OPT "-spn"
#        endif

#     else

#        if defined(_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#             define BOOST_LIB_RT_OPT "-sgyd"
#        elif defined(_DEBUG)
#             define BOOST_LIB_RT_OPT "-sgd"
#        else
#            define BOOST_LIB_RT_OPT "-s"
#        endif

#     endif

#  endif

#elif defined(__BORLANDC__)

//
// figure out whether we want the debug builds or not:
//
#if __BORLANDC__ > 0x561
#pragma defineonoption BOOST_BORLAND_DEBUG -v
#endif
//
// sanity check:
//
#if defined(__STL_DEBUG) || defined(_STLP_DEBUG)
#error "Pre-built versions of the Boost libraries are not provided in STLport-debug form"
#endif

#  ifdef _RTLDLL

#     if defined(BOOST_BORLAND_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#         define BOOST_LIB_RT_OPT "-yd"
#     elif defined(BOOST_BORLAND_DEBUG)
#         define BOOST_LIB_RT_OPT "-d"
#     elif defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#         define BOOST_LIB_RT_OPT -y
#     else
#         define BOOST_LIB_RT_OPT
#     endif

#  else

#     if defined(BOOST_BORLAND_DEBUG)\
               && defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#         define BOOST_LIB_RT_OPT "-syd"
#     elif defined(BOOST_BORLAND_DEBUG)
#         define BOOST_LIB_RT_OPT "-sd"
#     elif defined(BOOST_DEBUG_PYTHON) && defined(BOOST_LINKING_PYTHON)
#         define BOOST_LIB_RT_OPT "-sy"
#     else
#         define BOOST_LIB_RT_OPT "-s"
#     endif

#  endif

#endif

//
// BOOST_LIB_ARCH_AND_MODEL_OPT
//

#if defined( _M_IX86 )
#  define BOOST_LIB_ARCH_AND_MODEL_OPT "-x32"
#elif defined( _M_X64 )
#  define BOOST_LIB_ARCH_AND_MODEL_OPT "-x64"
#elif defined( _M_ARM )
#  define BOOST_LIB_ARCH_AND_MODEL_OPT "-a32"
#elif defined( _M_ARM64 )
#  define BOOST_LIB_ARCH_AND_MODEL_OPT "-a64"
#endif

//
// select linkage opt:
//
#if (defined(_DLL) || defined(_RTLDLL)) && defined(BOOST_DYN_LINK)
#  define BOOST_LIB_PREFIX
#elif defined(BOOST_DYN_LINK)
#  error "Mixing a dll boost library with a static runtime is a really bad idea..."
#else
#  define BOOST_LIB_PREFIX "lib"
#endif

//
// now include the lib:
//
#if defined(BOOST_LIB_NAME) \
      && defined(BOOST_LIB_PREFIX) \
      && defined(BOOST_LIB_TOOLSET) \
      && defined(BOOST_LIB_THREAD_OPT) \
      && defined(BOOST_LIB_RT_OPT) \
      && defined(BOOST_LIB_ARCH_AND_MODEL_OPT) \
      && defined(BOOST_LIB_VERSION)

#ifdef BOOST_AUTO_LINK_TAGGED
#  pragma comment(lib, BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT ".lib")
#  ifdef BOOST_LIB_DIAGNOSTIC
#     pragma message ("Linking to lib file: " BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT ".lib")
#  endif
#elif defined(BOOST_AUTO_LINK_SYSTEM)
#  pragma comment(lib, BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) ".lib")
#  ifdef BOOST_LIB_DIAGNOSTIC
#     pragma message ("Linking to lib file: " BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) ".lib")
#  endif
#elif defined(BOOST_AUTO_LINK_NOMANGLE)
#  pragma comment(lib, BOOST_STRINGIZE(BOOST_LIB_NAME) ".lib")
#  ifdef BOOST_LIB_DIAGNOSTIC
#     pragma message ("Linking to lib file: " BOOST_STRINGIZE(BOOST_LIB_NAME) ".lib")
#  endif
#elif defined(BOOST_LIB_BUILDID)
#  pragma comment(lib, BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) "-" BOOST_LIB_TOOLSET BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT "-" BOOST_LIB_VERSION "-" BOOST_STRINGIZE(BOOST_LIB_BUILDID) ".lib")
#  ifdef BOOST_LIB_DIAGNOSTIC
#     pragma message ("Linking to lib file: " BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) "-" BOOST_LIB_TOOLSET BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT "-" BOOST_LIB_VERSION "-" BOOST_STRINGIZE(BOOST_LIB_BUILDID) ".lib")
#  endif
#else
#  pragma comment(lib, BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) "-" BOOST_LIB_TOOLSET BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT "-" BOOST_LIB_VERSION ".lib")
#  ifdef BOOST_LIB_DIAGNOSTIC
#     pragma message ("Linking to lib file: " BOOST_LIB_PREFIX BOOST_STRINGIZE(BOOST_LIB_NAME) "-" BOOST_LIB_TOOLSET BOOST_LIB_THREAD_OPT BOOST_LIB_RT_OPT BOOST_LIB_ARCH_AND_MODEL_OPT "-" BOOST_LIB_VERSION ".lib")
#  endif
#endif

#else
#  error "some required macros where not defined (internal logic error)."
#endif


#endif // _MSC_VER || __BORLANDC__

//
// finally undef any macros we may have set:
//
#ifdef BOOST_LIB_PREFIX
#  undef BOOST_LIB_PREFIX
#endif
#if defined(BOOST_LIB_NAME)
#  undef BOOST_LIB_NAME
#endif
// Don't undef this one: it can be set by the user and should be the 
// same for all libraries:
//#if defined(BOOST_LIB_TOOLSET)
//#  undef BOOST_LIB_TOOLSET
//#endif
#if defined(BOOST_LIB_THREAD_OPT)
#  undef BOOST_LIB_THREAD_OPT
#endif
#if defined(BOOST_LIB_RT_OPT)
#  undef BOOST_LIB_RT_OPT
#endif
#if defined(BOOST_LIB_ARCH_AND_MODEL_OPT)
#  undef BOOST_LIB_ARCH_AND_MODEL_OPT
#endif
#if defined(BOOST_LIB_LINK_OPT)
#  undef BOOST_LIB_LINK_OPT
#endif
#if defined(BOOST_LIB_DEBUG_OPT)
#  undef BOOST_LIB_DEBUG_OPT
#endif
#if defined(BOOST_DYN_LINK)
#  undef BOOST_DYN_LINK
#endif


