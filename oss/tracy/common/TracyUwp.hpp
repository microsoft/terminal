#ifndef __TRACYUWP_HPP__
#define __TRACYUWP_HPP__

#ifdef _WIN32
#  include <winapifamily.h>
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#    define TRACY_UWP
#  endif
#endif

#endif
