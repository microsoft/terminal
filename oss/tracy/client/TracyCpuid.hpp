#ifndef __TRACYCPUID_HPP__
#define __TRACYCPUID_HPP__

// Prior to GCC 11 the cpuid.h header did not have any include guards and thus
// including it more than once would cause a compiler error due to symbol
// redefinitions. In order to support older GCC versions, we have to wrap this
// include between custom include guards to prevent this issue.
// See also https://github.com/wolfpld/tracy/issues/452

#include <cpuid.h>

#endif
