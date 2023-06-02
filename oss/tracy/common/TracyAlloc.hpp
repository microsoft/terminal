#ifndef __TRACYALLOC_HPP__
#define __TRACYALLOC_HPP__

#include <stdlib.h>

#if defined TRACY_ENABLE && !defined __EMSCRIPTEN__
#  include "TracyApi.h"
#  include "TracyForceInline.hpp"
#  include "../client/tracy_rpmalloc.hpp"
#  define TRACY_USE_RPMALLOC
#endif

namespace tracy
{

#ifdef TRACY_USE_RPMALLOC
TRACY_API void InitRpmalloc();
#else
static inline void InitRpmalloc() {}
#endif

static inline void* tracy_malloc( size_t size )
{
#ifdef TRACY_USE_RPMALLOC
    InitRpmalloc();
    return rpmalloc( size );
#else
    return malloc( size );
#endif
}

static inline void* tracy_malloc_fast( size_t size )
{
#ifdef TRACY_USE_RPMALLOC
    return rpmalloc( size );
#else
    return malloc( size );
#endif
}

static inline void tracy_free( void* ptr )
{
#ifdef TRACY_USE_RPMALLOC
    InitRpmalloc();
    rpfree( ptr );
#else
    free( ptr );
#endif
}

static inline void tracy_free_fast( void* ptr )
{
#ifdef TRACY_USE_RPMALLOC
    rpfree( ptr );
#else
    free( ptr );
#endif
}

static inline void* tracy_realloc( void* ptr, size_t size )
{
#ifdef TRACY_USE_RPMALLOC
    InitRpmalloc();
    return rprealloc( ptr, size );
#else
    return realloc( ptr, size );
#endif
}

}

#endif
