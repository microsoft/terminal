#ifndef __TRACYSTRINGHELPERS_HPP__
#define __TRACYSTRINGHELPERS_HPP__

#include <assert.h>
#include <string.h>

#include "../common/TracyAlloc.hpp"
#include "../common/TracyForceInline.hpp"

namespace tracy
{

static tracy_force_inline char* CopyString( const char* src, size_t sz )
{
    auto dst = (char*)tracy_malloc( sz + 1 );
    memcpy( dst, src, sz );
    dst[sz] = '\0';
    return dst;
}

static tracy_force_inline char* CopyString( const char* src )
{
    return CopyString( src, strlen( src ) );
}

static tracy_force_inline char* CopyStringFast( const char* src, size_t sz )
{
    auto dst = (char*)tracy_malloc_fast( sz + 1 );
    memcpy( dst, src, sz );
    dst[sz] = '\0';
    return dst;
}

static tracy_force_inline char* CopyStringFast( const char* src )
{
    return CopyStringFast( src, strlen( src ) );
}

}

#endif
