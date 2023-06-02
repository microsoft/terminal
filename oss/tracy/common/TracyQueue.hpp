#ifndef __TRACYQUEUE_HPP__
#define __TRACYQUEUE_HPP__

#include <stddef.h>
#include <stdint.h>

namespace tracy
{

enum class QueueType : uint8_t
{
    ZoneText,
    ZoneName,
    Message,
    MessageColor,
    MessageCallstack,
    MessageColorCallstack,
    MessageAppInfo,
    ZoneBeginAllocSrcLoc,
    ZoneBeginAllocSrcLocCallstack,
    CallstackSerial,
    Callstack,
    CallstackAlloc,
    CallstackSample,
    CallstackSampleContextSwitch,
    FrameImage,
    ZoneBegin,
    ZoneBeginCallstack,
    ZoneEnd,
    LockWait,
    LockObtain,
    LockRelease,
    LockSharedWait,
    LockSharedObtain,
    LockSharedRelease,
    LockName,
    MemAlloc,
    MemAllocNamed,
    MemFree,
    MemFreeNamed,
    MemAllocCallstack,
    MemAllocCallstackNamed,
    MemFreeCallstack,
    MemFreeCallstackNamed,
    GpuZoneBegin,
    GpuZoneBeginCallstack,
    GpuZoneBeginAllocSrcLoc,
    GpuZoneBeginAllocSrcLocCallstack,
    GpuZoneEnd,
    GpuZoneBeginSerial,
    GpuZoneBeginCallstackSerial,
    GpuZoneBeginAllocSrcLocSerial,
    GpuZoneBeginAllocSrcLocCallstackSerial,
    GpuZoneEndSerial,
    PlotDataInt,
    PlotDataFloat,
    PlotDataDouble,
    ContextSwitch,
    ThreadWakeup,
    GpuTime,
    GpuContextName,
    CallstackFrameSize,
    SymbolInformation,
    ExternalNameMetadata,
    SymbolCodeMetadata,
    SourceCodeMetadata,
    FiberEnter,
    FiberLeave,
    Terminate,
    KeepAlive,
    ThreadContext,
    GpuCalibration,
    Crash,
    CrashReport,
    ZoneValidation,
    ZoneColor,
    ZoneValue,
    FrameMarkMsg,
    FrameMarkMsgStart,
    FrameMarkMsgEnd,
    FrameVsync,
    SourceLocation,
    LockAnnounce,
    LockTerminate,
    LockMark,
    MessageLiteral,
    MessageLiteralColor,
    MessageLiteralCallstack,
    MessageLiteralColorCallstack,
    GpuNewContext,
    CallstackFrame,
    SysTimeReport,
    TidToPid,
    HwSampleCpuCycle,
    HwSampleInstructionRetired,
    HwSampleCacheReference,
    HwSampleCacheMiss,
    HwSampleBranchRetired,
    HwSampleBranchMiss,
    PlotConfig,
    ParamSetup,
    AckServerQueryNoop,
    AckSourceCodeNotAvailable,
    AckSymbolCodeNotAvailable,
    CpuTopology,
    SingleStringData,
    SecondStringData,
    MemNamePayload,
    StringData,
    ThreadName,
    PlotName,
    SourceLocationPayload,
    CallstackPayload,
    CallstackAllocPayload,
    FrameName,
    FrameImageData,
    ExternalName,
    ExternalThreadName,
    SymbolCode,
    SourceCode,
    FiberName,
    NUM_TYPES
};

#pragma pack( push, 1 )

struct QueueThreadContext
{
    uint32_t thread;
};

struct QueueZoneBeginLean
{
    int64_t time;
};

struct QueueZoneBegin : public QueueZoneBeginLean
{
    uint64_t srcloc;    // ptr
};

struct QueueZoneBeginThread : public QueueZoneBegin
{
    uint32_t thread;
};

struct QueueZoneEnd
{
    int64_t time;
};

struct QueueZoneEndThread : public QueueZoneEnd
{
    uint32_t thread;
};

struct QueueZoneValidation
{
    uint32_t id;
};

struct QueueZoneValidationThread : public QueueZoneValidation
{
    uint32_t thread;
};

struct QueueZoneColor
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
};

struct QueueZoneColorThread : public QueueZoneColor
{
    uint32_t thread;
};

struct QueueZoneValue
{
    uint64_t value;
};

struct QueueZoneValueThread : public QueueZoneValue
{
    uint32_t thread;
};

struct QueueStringTransfer
{
    uint64_t ptr;
};

struct QueueFrameMark
{
    int64_t time;
    uint64_t name;      // ptr
};

struct QueueFrameVsync
{
    int64_t time;
    uint32_t id;
};

struct QueueFrameImage
{
    uint32_t frame;
    uint16_t w;
    uint16_t h;
    uint8_t flip;
};

struct QueueFrameImageFat : public QueueFrameImage
{
    uint64_t image;     // ptr
};

struct QueueSourceLocation
{
    uint64_t name;
    uint64_t function;  // ptr
    uint64_t file;      // ptr
    uint32_t line;
    uint8_t b;
    uint8_t g;
    uint8_t r;
};

struct QueueZoneTextFat
{
    uint64_t text;      // ptr
    uint16_t size;
};

struct QueueZoneTextFatThread : public QueueZoneTextFat
{
    uint32_t thread;
};

enum class LockType : uint8_t
{
    Lockable,
    SharedLockable
};

struct QueueLockAnnounce
{
    uint32_t id;
    int64_t time;
    uint64_t lckloc;    // ptr
    LockType type;
};

struct QueueFiberEnter
{
    int64_t time;
    uint64_t fiber;     // ptr
    uint32_t thread;
};

struct QueueFiberLeave
{
    int64_t time;
    uint32_t thread;
};

struct QueueLockTerminate
{
    uint32_t id;
    int64_t time;
};

struct QueueLockWait
{
    uint32_t thread;
    uint32_t id;
    int64_t time;
};

struct QueueLockObtain
{
    uint32_t thread;
    uint32_t id;
    int64_t time;
};

struct QueueLockRelease
{
    uint32_t id;
    int64_t time;
};

struct QueueLockReleaseShared : public QueueLockRelease
{
    uint32_t thread;
};

struct QueueLockMark
{
    uint32_t thread;
    uint32_t id;
    uint64_t srcloc;    // ptr
};

struct QueueLockName
{
    uint32_t id;
};

struct QueueLockNameFat : public QueueLockName
{
    uint64_t name;      // ptr
    uint16_t size;
};

struct QueuePlotDataBase
{
    uint64_t name;      // ptr
    int64_t time;
};

struct QueuePlotDataInt : public QueuePlotDataBase
{
    int64_t val;
};

struct QueuePlotDataFloat : public QueuePlotDataBase 
{
    float val;
};

struct QueuePlotDataDouble : public QueuePlotDataBase
{
    double val;
};

struct QueueMessage
{
    int64_t time;
};

struct QueueMessageColor : public QueueMessage
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
};

struct QueueMessageLiteral : public QueueMessage
{
    uint64_t text;      // ptr
};

struct QueueMessageLiteralThread : public QueueMessageLiteral
{
    uint32_t thread;
};

struct QueueMessageColorLiteral : public QueueMessageColor
{
    uint64_t text;      // ptr
};

struct QueueMessageColorLiteralThread : public QueueMessageColorLiteral
{
    uint32_t thread;
};

struct QueueMessageFat : public QueueMessage
{
    uint64_t text;      // ptr
    uint16_t size;
};

struct QueueMessageFatThread : public QueueMessageFat
{
    uint32_t thread;
};

struct QueueMessageColorFat : public QueueMessageColor
{
    uint64_t text;      // ptr
    uint16_t size;
};

struct QueueMessageColorFatThread : public QueueMessageColorFat
{
    uint32_t thread;
};

// Don't change order, only add new entries at the end, this is also used on trace dumps!
enum class GpuContextType : uint8_t
{
    Invalid,
    OpenGl,
    Vulkan,
    OpenCL,
    Direct3D12,
    Direct3D11
};

enum GpuContextFlags : uint8_t
{
    GpuContextCalibration   = 1 << 0
};

struct QueueGpuNewContext
{
    int64_t cpuTime;
    int64_t gpuTime;
    uint32_t thread;
    float period;
    uint8_t context;
    GpuContextFlags flags;
    GpuContextType type;
};

struct QueueGpuZoneBeginLean
{
    int64_t cpuTime;
    uint32_t thread;
    uint16_t queryId;
    uint8_t context;
};

struct QueueGpuZoneBegin : public QueueGpuZoneBeginLean
{
    uint64_t srcloc;
};

struct QueueGpuZoneEnd
{
    int64_t cpuTime;
    uint32_t thread;
    uint16_t queryId;
    uint8_t context;
};

struct QueueGpuTime
{
    int64_t gpuTime;
    uint16_t queryId;
    uint8_t context;
};

struct QueueGpuCalibration
{
    int64_t gpuTime;
    int64_t cpuTime;
    int64_t cpuDelta;
    uint8_t context;
};

struct QueueGpuContextName
{
    uint8_t context;
};

struct QueueGpuContextNameFat : public QueueGpuContextName
{
    uint64_t ptr;
    uint16_t size;
};

struct QueueMemNamePayload
{
    uint64_t name;
};

struct QueueMemAlloc
{
    int64_t time;
    uint32_t thread;
    uint64_t ptr;
    char size[6];
};

struct QueueMemFree
{
    int64_t time;
    uint32_t thread;
    uint64_t ptr;
};

struct QueueCallstackFat
{
    uint64_t ptr;
};

struct QueueCallstackFatThread : public QueueCallstackFat
{
    uint32_t thread;
};

struct QueueCallstackAllocFat
{
    uint64_t ptr;
    uint64_t nativePtr;
};

struct QueueCallstackAllocFatThread : public QueueCallstackAllocFat
{
    uint32_t thread;
};

struct QueueCallstackSample
{
    int64_t time;
    uint32_t thread;
};

struct QueueCallstackSampleFat : public QueueCallstackSample
{
    uint64_t ptr;
};

struct QueueCallstackFrameSize
{
    uint64_t ptr;
    uint8_t size;
};

struct QueueCallstackFrameSizeFat : public QueueCallstackFrameSize
{
    uint64_t data;
    uint64_t imageName;
};

struct QueueCallstackFrame
{
    uint32_t line;
    uint64_t symAddr;
    uint32_t symLen;
};

struct QueueSymbolInformation
{
    uint32_t line;
    uint64_t symAddr;
};

struct QueueSymbolInformationFat : public QueueSymbolInformation
{
    uint64_t fileString;
    uint8_t needFree;
};

struct QueueCrashReport
{
    int64_t time;
    uint64_t text;      // ptr
};

struct QueueCrashReportThread
{
    uint32_t thread;
};

struct QueueSysTime
{
    int64_t time;
    float sysTime;
};

struct QueueContextSwitch
{
    int64_t time;
    uint32_t oldThread;
    uint32_t newThread;
    uint8_t cpu;
    uint8_t reason;
    uint8_t state;
};

struct QueueThreadWakeup
{
    int64_t time;
    uint32_t thread;
};

struct QueueTidToPid
{
    uint64_t tid;
    uint64_t pid;
};

struct QueueHwSample
{
    uint64_t ip;
    int64_t time;
};

enum class PlotFormatType : uint8_t
{
    Number,
    Memory,
    Percentage
};

struct QueuePlotConfig
{
    uint64_t name;      // ptr
    uint8_t type;
    uint8_t step;
    uint8_t fill;
    uint32_t color;
};

struct QueueParamSetup
{
    uint32_t idx;
    uint64_t name;      // ptr
    uint8_t isBool;
    int32_t val;
};

struct QueueSourceCodeNotAvailable
{
    uint32_t id;
};

struct QueueCpuTopology
{
    uint32_t package;
    uint32_t core;
    uint32_t thread;
};

struct QueueExternalNameMetadata
{
    uint64_t thread;
    uint64_t name;
    uint64_t threadName;
};

struct QueueSymbolCodeMetadata
{
    uint64_t symbol;
    uint64_t ptr;
    uint32_t size;
};

struct QueueSourceCodeMetadata
{
    uint64_t ptr;
    uint32_t size;
    uint32_t id;
};

struct QueueHeader
{
    union
    {
        QueueType type;
        uint8_t idx;
    };
};

struct QueueItem
{
    QueueHeader hdr;
    union
    {
        QueueThreadContext threadCtx;
        QueueZoneBegin zoneBegin;
        QueueZoneBeginLean zoneBeginLean;
        QueueZoneBeginThread zoneBeginThread;
        QueueZoneEnd zoneEnd;
        QueueZoneEndThread zoneEndThread;
        QueueZoneValidation zoneValidation;
        QueueZoneValidationThread zoneValidationThread;
        QueueZoneColor zoneColor;
        QueueZoneColorThread zoneColorThread;
        QueueZoneValue zoneValue;
        QueueZoneValueThread zoneValueThread;
        QueueStringTransfer stringTransfer;
        QueueFrameMark frameMark;
        QueueFrameVsync frameVsync;
        QueueFrameImage frameImage;
        QueueFrameImageFat frameImageFat;
        QueueSourceLocation srcloc;
        QueueZoneTextFat zoneTextFat;
        QueueZoneTextFatThread zoneTextFatThread;
        QueueLockAnnounce lockAnnounce;
        QueueLockTerminate lockTerminate;
        QueueLockWait lockWait;
        QueueLockObtain lockObtain;
        QueueLockRelease lockRelease;
        QueueLockReleaseShared lockReleaseShared;
        QueueLockMark lockMark;
        QueueLockName lockName;
        QueueLockNameFat lockNameFat;
        QueuePlotDataInt plotDataInt;
        QueuePlotDataFloat plotDataFloat;
        QueuePlotDataDouble plotDataDouble;
        QueueMessage message;
        QueueMessageColor messageColor;
        QueueMessageLiteral messageLiteral;
        QueueMessageLiteralThread messageLiteralThread;
        QueueMessageColorLiteral messageColorLiteral;
        QueueMessageColorLiteralThread messageColorLiteralThread;
        QueueMessageFat messageFat;
        QueueMessageFatThread messageFatThread;
        QueueMessageColorFat messageColorFat;
        QueueMessageColorFatThread messageColorFatThread;
        QueueGpuNewContext gpuNewContext;
        QueueGpuZoneBegin gpuZoneBegin;
        QueueGpuZoneBeginLean gpuZoneBeginLean;
        QueueGpuZoneEnd gpuZoneEnd;
        QueueGpuTime gpuTime;
        QueueGpuCalibration gpuCalibration;
        QueueGpuContextName gpuContextName;
        QueueGpuContextNameFat gpuContextNameFat;
        QueueMemAlloc memAlloc;
        QueueMemFree memFree;
        QueueMemNamePayload memName;
        QueueCallstackFat callstackFat;
        QueueCallstackFatThread callstackFatThread;
        QueueCallstackAllocFat callstackAllocFat;
        QueueCallstackAllocFatThread callstackAllocFatThread;
        QueueCallstackSample callstackSample;
        QueueCallstackSampleFat callstackSampleFat;
        QueueCallstackFrameSize callstackFrameSize;
        QueueCallstackFrameSizeFat callstackFrameSizeFat;
        QueueCallstackFrame callstackFrame;
        QueueSymbolInformation symbolInformation;
        QueueSymbolInformationFat symbolInformationFat;
        QueueCrashReport crashReport;
        QueueCrashReportThread crashReportThread;
        QueueSysTime sysTime;
        QueueContextSwitch contextSwitch;
        QueueThreadWakeup threadWakeup;
        QueueTidToPid tidToPid;
        QueueHwSample hwSample;
        QueuePlotConfig plotConfig;
        QueueParamSetup paramSetup;
        QueueCpuTopology cpuTopology;
        QueueExternalNameMetadata externalNameMetadata;
        QueueSymbolCodeMetadata symbolCodeMetadata;
        QueueSourceCodeMetadata sourceCodeMetadata;
        QueueSourceCodeNotAvailable sourceCodeNotAvailable;
        QueueFiberEnter fiberEnter;
        QueueFiberLeave fiberLeave;
    };
};
#pragma pack( pop )


enum { QueueItemSize = sizeof( QueueItem ) };

static constexpr size_t QueueDataSize[] = {
    sizeof( QueueHeader ),                                  // zone text
    sizeof( QueueHeader ),                                  // zone name
    sizeof( QueueHeader ) + sizeof( QueueMessage ),
    sizeof( QueueHeader ) + sizeof( QueueMessageColor ),
    sizeof( QueueHeader ) + sizeof( QueueMessage ),         // callstack
    sizeof( QueueHeader ) + sizeof( QueueMessageColor ),    // callstack
    sizeof( QueueHeader ) + sizeof( QueueMessage ),         // app info
    sizeof( QueueHeader ) + sizeof( QueueZoneBeginLean ),   // allocated source location
    sizeof( QueueHeader ) + sizeof( QueueZoneBeginLean ),   // allocated source location, callstack
    sizeof( QueueHeader ),                                  // callstack memory
    sizeof( QueueHeader ),                                  // callstack
    sizeof( QueueHeader ),                                  // callstack alloc
    sizeof( QueueHeader ) + sizeof( QueueCallstackSample ),
    sizeof( QueueHeader ) + sizeof( QueueCallstackSample ), // context switch
    sizeof( QueueHeader ) + sizeof( QueueFrameImage ),
    sizeof( QueueHeader ) + sizeof( QueueZoneBegin ),
    sizeof( QueueHeader ) + sizeof( QueueZoneBegin ),       // callstack
    sizeof( QueueHeader ) + sizeof( QueueZoneEnd ),
    sizeof( QueueHeader ) + sizeof( QueueLockWait ),
    sizeof( QueueHeader ) + sizeof( QueueLockObtain ),
    sizeof( QueueHeader ) + sizeof( QueueLockRelease ),
    sizeof( QueueHeader ) + sizeof( QueueLockWait ),        // shared
    sizeof( QueueHeader ) + sizeof( QueueLockObtain ),      // shared
    sizeof( QueueHeader ) + sizeof( QueueLockReleaseShared ),
    sizeof( QueueHeader ) + sizeof( QueueLockName ),
    sizeof( QueueHeader ) + sizeof( QueueMemAlloc ),
    sizeof( QueueHeader ) + sizeof( QueueMemAlloc ),        // named
    sizeof( QueueHeader ) + sizeof( QueueMemFree ),
    sizeof( QueueHeader ) + sizeof( QueueMemFree ),         // named
    sizeof( QueueHeader ) + sizeof( QueueMemAlloc ),        // callstack
    sizeof( QueueHeader ) + sizeof( QueueMemAlloc ),        // callstack, named
    sizeof( QueueHeader ) + sizeof( QueueMemFree ),         // callstack
    sizeof( QueueHeader ) + sizeof( QueueMemFree ),         // callstack, named
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBegin ),
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBegin ),    // callstack
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBeginLean ),// allocated source location
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBeginLean ),// allocated source location, callstack
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneEnd ),
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBegin ),    // serial
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBegin ),    // serial, callstack
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBeginLean ),// serial, allocated source location
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneBeginLean ),// serial, allocated source location, callstack
    sizeof( QueueHeader ) + sizeof( QueueGpuZoneEnd ),      // serial
    sizeof( QueueHeader ) + sizeof( QueuePlotDataInt ),
    sizeof( QueueHeader ) + sizeof( QueuePlotDataFloat ),
    sizeof( QueueHeader ) + sizeof( QueuePlotDataDouble ),
    sizeof( QueueHeader ) + sizeof( QueueContextSwitch ),
    sizeof( QueueHeader ) + sizeof( QueueThreadWakeup ),
    sizeof( QueueHeader ) + sizeof( QueueGpuTime ),
    sizeof( QueueHeader ) + sizeof( QueueGpuContextName ),
    sizeof( QueueHeader ) + sizeof( QueueCallstackFrameSize ),
    sizeof( QueueHeader ) + sizeof( QueueSymbolInformation ),
    sizeof( QueueHeader ),                                  // ExternalNameMetadata - not for wire transfer
    sizeof( QueueHeader ),                                  // SymbolCodeMetadata - not for wire transfer
    sizeof( QueueHeader ),                                  // SourceCodeMetadata - not for wire transfer
    sizeof( QueueHeader ) + sizeof( QueueFiberEnter ),
    sizeof( QueueHeader ) + sizeof( QueueFiberLeave ),
    // above items must be first
    sizeof( QueueHeader ),                                  // terminate
    sizeof( QueueHeader ),                                  // keep alive
    sizeof( QueueHeader ) + sizeof( QueueThreadContext ),
    sizeof( QueueHeader ) + sizeof( QueueGpuCalibration ),
    sizeof( QueueHeader ),                                  // crash
    sizeof( QueueHeader ) + sizeof( QueueCrashReport ),
    sizeof( QueueHeader ) + sizeof( QueueZoneValidation ),
    sizeof( QueueHeader ) + sizeof( QueueZoneColor ),
    sizeof( QueueHeader ) + sizeof( QueueZoneValue ),
    sizeof( QueueHeader ) + sizeof( QueueFrameMark ),       // continuous frames
    sizeof( QueueHeader ) + sizeof( QueueFrameMark ),       // start
    sizeof( QueueHeader ) + sizeof( QueueFrameMark ),       // end
    sizeof( QueueHeader ) + sizeof( QueueFrameVsync ),
    sizeof( QueueHeader ) + sizeof( QueueSourceLocation ),
    sizeof( QueueHeader ) + sizeof( QueueLockAnnounce ),
    sizeof( QueueHeader ) + sizeof( QueueLockTerminate ),
    sizeof( QueueHeader ) + sizeof( QueueLockMark ),
    sizeof( QueueHeader ) + sizeof( QueueMessageLiteral ),
    sizeof( QueueHeader ) + sizeof( QueueMessageColorLiteral ),
    sizeof( QueueHeader ) + sizeof( QueueMessageLiteral ),  // callstack
    sizeof( QueueHeader ) + sizeof( QueueMessageColorLiteral ), // callstack
    sizeof( QueueHeader ) + sizeof( QueueGpuNewContext ),
    sizeof( QueueHeader ) + sizeof( QueueCallstackFrame ),
    sizeof( QueueHeader ) + sizeof( QueueSysTime ),
    sizeof( QueueHeader ) + sizeof( QueueTidToPid ),
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // cpu cycle
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // instruction retired
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // cache reference
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // cache miss
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // branch retired
    sizeof( QueueHeader ) + sizeof( QueueHwSample ),        // branch miss
    sizeof( QueueHeader ) + sizeof( QueuePlotConfig ),
    sizeof( QueueHeader ) + sizeof( QueueParamSetup ),
    sizeof( QueueHeader ),                                  // server query acknowledgement
    sizeof( QueueHeader ) + sizeof( QueueSourceCodeNotAvailable ),
    sizeof( QueueHeader ),                                  // symbol code not available
    sizeof( QueueHeader ) + sizeof( QueueCpuTopology ),
    sizeof( QueueHeader ),                                  // single string data
    sizeof( QueueHeader ),                                  // second string data
    sizeof( QueueHeader ) + sizeof( QueueMemNamePayload ),
    // keep all QueueStringTransfer below
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // string data
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // thread name
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // plot name
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // allocated source location payload
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // callstack payload
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // callstack alloc payload
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // frame name
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // frame image data
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // external name
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // external thread name
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // symbol code
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // source code
    sizeof( QueueHeader ) + sizeof( QueueStringTransfer ),  // fiber name
};

static_assert( QueueItemSize == 32, "Queue item size not 32 bytes" );
static_assert( sizeof( QueueDataSize ) / sizeof( size_t ) == (uint8_t)QueueType::NUM_TYPES, "QueueDataSize mismatch" );
static_assert( sizeof( void* ) <= sizeof( uint64_t ), "Pointer size > 8 bytes" );
static_assert( sizeof( void* ) == sizeof( uintptr_t ), "Pointer size != uintptr_t" );

}

#endif
