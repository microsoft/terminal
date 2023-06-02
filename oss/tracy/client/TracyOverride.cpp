#ifdef TRACY_ENABLE
#  ifdef __linux__
#    include "TracyDebug.hpp"
#    ifdef TRACY_VERBOSE
#      include <dlfcn.h>
#      include <link.h>
#    endif

extern "C" int dlclose( void* hnd )
{
#ifdef TRACY_VERBOSE
    struct link_map* lm;
    if( dlinfo( hnd, RTLD_DI_LINKMAP, &lm ) == 0 )
    {
        TracyDebug( "Overriding dlclose for %s\n", lm->l_name );
    }
    else
    {
        TracyDebug( "Overriding dlclose for unknown object (%s)\n", dlerror() );
    }
#endif
    return 0;
}

#  endif
#endif
