#ifndef __TRACYCALLSTACK_H__
#define __TRACYCALLSTACK_H__

#ifndef TRACY_NO_CALLSTACK

#  if !defined _WIN32
#    include <sys/param.h>
#  endif

#  if defined _WIN32
#    include "../common/TracyUwp.hpp"
#    ifndef TRACY_UWP
#      define TRACY_HAS_CALLSTACK 1
#    endif
#  elif defined __ANDROID__
#    if !defined __arm__ || __ANDROID_API__ >= 21
#      define TRACY_HAS_CALLSTACK 2
#    else
#      define TRACY_HAS_CALLSTACK 5
#    endif
#  elif defined __linux
#    if defined _GNU_SOURCE && defined __GLIBC__
#      define TRACY_HAS_CALLSTACK 3
#    else
#      define TRACY_HAS_CALLSTACK 2
#    endif
#  elif defined __APPLE__
#    define TRACY_HAS_CALLSTACK 4
#  elif defined BSD
#    define TRACY_HAS_CALLSTACK 6
#  endif

#endif

#endif
