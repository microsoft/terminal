#ifndef __TRACYYIELD_HPP__
#define __TRACYYIELD_HPP__

#if defined __SSE2__ || defined _M_AMD64 || (defined _M_IX86_FP && _M_IX86_FP == 2)
#  include <emmintrin.h>
#else
#  include <thread>
#endif

#include "TracyForceInline.hpp"

namespace tracy
{

static tracy_force_inline void YieldThread()
{
#if defined __SSE2__ || defined _M_AMD64 || (defined _M_IX86_FP && _M_IX86_FP == 2)
    _mm_pause();
#elif defined __aarch64__
    asm volatile( "isb" : : );
#else
    std::this_thread::yield();
#endif
}

}

#endif
