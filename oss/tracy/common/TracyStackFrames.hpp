#ifndef __TRACYSTACKFRAMES_HPP__
#define __TRACYSTACKFRAMES_HPP__

#include <stddef.h>

namespace tracy
{

struct StringMatch
{
    const char* str;
    size_t len;
};

extern const char** s_tracyStackFrames;
extern const StringMatch* s_tracySkipSubframes;

static constexpr int s_tracySkipSubframesMinLen = 7;

}

#endif
