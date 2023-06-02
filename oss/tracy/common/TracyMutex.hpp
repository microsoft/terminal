#ifndef __TRACYMUTEX_HPP__
#define __TRACYMUTEX_HPP__

#if defined _MSC_VER

#  include <shared_mutex>

namespace tracy
{
using TracyMutex = std::shared_mutex;
}

#else

#include <mutex>

namespace tracy
{
using TracyMutex = std::mutex;
}

#endif

#endif
