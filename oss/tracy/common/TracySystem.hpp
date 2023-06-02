#ifndef __TRACYSYSTEM_HPP__
#define __TRACYSYSTEM_HPP__

#include <stdint.h>

#include "TracyApi.h"

namespace tracy
{

namespace detail
{
TRACY_API uint32_t GetThreadHandleImpl();
}

#ifdef TRACY_ENABLE
TRACY_API uint32_t GetThreadHandle();
#else
static inline uint32_t GetThreadHandle()
{
    return detail::GetThreadHandleImpl();
}
#endif

TRACY_API void SetThreadName( const char* name );
TRACY_API const char* GetThreadName( uint32_t id );

TRACY_API const char* GetEnvVar(const char* name);

}

#endif
