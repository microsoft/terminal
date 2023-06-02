#ifndef __TRACYCALLSTACK_HPP__
#define __TRACYCALLSTACK_HPP__

#include "../common/TracyApi.h"
#include "../common/TracyForceInline.hpp"
#include "TracyCallstack.h"

#if TRACY_HAS_CALLSTACK == 2 || TRACY_HAS_CALLSTACK == 5
#  include <unwind.h>
#elif TRACY_HAS_CALLSTACK >= 3
#  include <execinfo.h>
#endif


#ifndef TRACY_HAS_CALLSTACK

namespace tracy
{
static tracy_force_inline void* Callstack( int depth ) { return nullptr; }
}

#else

#ifdef TRACY_DEBUGINFOD
#  include <elfutils/debuginfod.h>
#endif

#include <assert.h>
#include <stdint.h>

#include "../common/TracyAlloc.hpp"

namespace tracy
{

struct CallstackSymbolData
{
    const char* file;
    uint32_t line;
    bool needFree;
    uint64_t symAddr;
};

struct CallstackEntry
{
    const char* name;
    const char* file;
    uint32_t line;
    uint32_t symLen;
    uint64_t symAddr;
};

struct CallstackEntryData
{
    const CallstackEntry* data;
    uint8_t size;
    const char* imageName;
};

CallstackSymbolData DecodeSymbolAddress( uint64_t ptr );
const char* DecodeCallstackPtrFast( uint64_t ptr );
CallstackEntryData DecodeCallstackPtr( uint64_t ptr );
void InitCallstack();
void InitCallstackCritical();
void EndCallstack();
const char* GetKernelModulePath( uint64_t addr );

#ifdef TRACY_DEBUGINFOD
const uint8_t* GetBuildIdForImage( const char* image, size_t& size );
debuginfod_client* GetDebuginfodClient();
#endif

#if TRACY_HAS_CALLSTACK == 1

extern "C"
{
    typedef unsigned long (__stdcall *___tracy_t_RtlWalkFrameChain)( void**, unsigned long, unsigned long );
    TRACY_API extern ___tracy_t_RtlWalkFrameChain ___tracy_RtlWalkFrameChain;
}

static tracy_force_inline void* Callstack( int depth )
{
    assert( depth >= 1 && depth < 63 );
    auto trace = (uintptr_t*)tracy_malloc( ( 1 + depth ) * sizeof( uintptr_t ) );
    const auto num = ___tracy_RtlWalkFrameChain( (void**)( trace + 1 ), depth, 0 );
    *trace = num;
    return trace;
}

#elif TRACY_HAS_CALLSTACK == 2 || TRACY_HAS_CALLSTACK == 5

struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code tracy_unwind_callback( struct _Unwind_Context* ctx, void* arg )
{
    auto state = (BacktraceState*)arg;
    uintptr_t pc = _Unwind_GetIP( ctx );
    if( pc )
    {
        if( state->current == state->end ) return _URC_END_OF_STACK;
        *state->current++ = (void*)pc;
    }
    return _URC_NO_REASON;
}

static tracy_force_inline void* Callstack( int depth )
{
    assert( depth >= 1 && depth < 63 );

    auto trace = (uintptr_t*)tracy_malloc( ( 1 + depth ) * sizeof( uintptr_t ) );
    BacktraceState state = { (void**)(trace+1), (void**)(trace+1+depth) };
    _Unwind_Backtrace( tracy_unwind_callback, &state );

    *trace = (uintptr_t*)state.current - trace + 1;

    return trace;
}

#elif TRACY_HAS_CALLSTACK == 3 || TRACY_HAS_CALLSTACK == 4 || TRACY_HAS_CALLSTACK == 6

static tracy_force_inline void* Callstack( int depth )
{
    assert( depth >= 1 );

    auto trace = (uintptr_t*)tracy_malloc( ( 1 + (size_t)depth ) * sizeof( uintptr_t ) );
    const auto num = (size_t)backtrace( (void**)(trace+1), depth );
    *trace = num;

    return trace;
}

#endif

}

#endif

#endif
