#ifndef __TRACYSYSTIME_HPP__
#define __TRACYSYSTIME_HPP__

#if defined _WIN32 || defined __linux__ || defined __APPLE__
#  define TRACY_HAS_SYSTIME
#else
#  include <sys/param.h>
#endif

#ifdef BSD
#  define TRACY_HAS_SYSTIME
#endif

#ifdef TRACY_HAS_SYSTIME

#include <stdint.h>

namespace tracy
{

class SysTime
{
public:
    SysTime();
    float Get();

    void ReadTimes();

private:
    uint64_t idle, used;
};

}
#endif

#endif
