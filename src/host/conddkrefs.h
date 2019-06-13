/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conddkrefs.h

Abstract:
- Contains headers that are a part of the public DDK.
- We don't include both the DDK and the SDK at the same time because they mesh poorly
and it's easier to include a copy of the infrequently changing defs here.
--*/

#pragma once

#ifndef _DDK_INCLUDED

#pragma region wdm.h(public DDK)
//
// Define the base asynchronous I/O argument types
//
extern "C" {

//
// ClientId
//

typedef struct _CLIENT_ID
{
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID* PCLIENT_ID;

// POBJECT_ATTRIBUTES

//
// Unicode strings are counted 16-bit character strings. If they are
// NULL terminated, Length does not include trailing NULL.
//

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
#ifdef MIDL_PASS
    [size_is(MaximumLength / 2), length_is((Length) / 2)] USHORT* Buffer;
#else // MIDL_PASS
    _Field_size_bytes_part_(MaximumLength, Length) PWCH Buffer;
#endif // MIDL_PASS
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

// OBJECT_ATTRIBUTES

// clang-format off
#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_FORCE_ACCESS_CHECK  0x00000400L
#define OBJ_VALID_ATTRIBUTES    0x000007F2L
// clang-format on

typedef struct _OBJECT_ATTRIBUTES
{
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor; // Points to type SECURITY_DESCRIPTOR
    PVOID SecurityQualityOfService; // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES* POBJECT_ATTRIBUTES;
typedef CONST OBJECT_ATTRIBUTES* PCOBJECT_ATTRIBUTES;

//++
//
// VOID
// InitializeObjectAttributes(
//     _Out_ POBJECT_ATTRIBUTES p,
//     _In_ PUNICODE_STRING n,
//     _In_ ULONG a,
//     _In_ HANDLE r,
//     _In_ PSECURITY_DESCRIPTOR s
//     )
//
//--

#define InitializeObjectAttributes(p, n, a, r, s) \
    {                                             \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES);  \
        (p)->RootDirectory = r;                   \
        (p)->Attributes = a;                      \
        (p)->ObjectName = n;                      \
        (p)->SecurityDescriptor = s;              \
        (p)->SecurityQualityOfService = NULL;     \
    }

// UNICODE_STRING

// OBJ_CASE_INSENSITIVE
// OBJ_INHERIT
// InitializeObjectAttributes

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID Pointer;
    } DUMMYUNIONNAME;

    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

//
// Define the file system information class values
//
// WARNING:  The order of the following values are assumed by the I/O system.
//           Any changes made here should be reflected there as well.

// clang-format off
    typedef enum _FSINFOCLASS {
        FileFsVolumeInformation = 1,
        FileFsLabelInformation,         // 2
        FileFsSizeInformation,          // 3
        FileFsDeviceInformation,        // 4
        FileFsAttributeInformation,     // 5
        FileFsControlInformation,       // 6
        FileFsFullSizeInformation,      // 7
        FileFsObjectIdInformation,      // 8
        FileFsDriverPathInformation,    // 9
        FileFsVolumeFlagsInformation,   // 10
        FileFsSectorSizeInformation,    // 11
        FileFsDataCopyInformation,      // 12
        FileFsMetadataSizeInformation,  // 13
        FileFsMaximumInformation
    } FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;
// clang-format on

#ifndef DEVICE_TYPE
#define DEVICE_TYPE DWORD
#endif

typedef struct _FILE_FS_DEVICE_INFORMATION
{
    DEVICE_TYPE DeviceType;
    ULONG Characteristics;
} FILE_FS_DEVICE_INFORMATION, *PFILE_FS_DEVICE_INFORMATION;

#pragma region IOCTL codes
//
// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-32767, and 32768-65535 are reserved for use
// by customers.
//

#ifndef FILE_DEVICE_CONSOLE
#define FILE_DEVICE_CONSOLE 0x00000050
#endif

//
// Macro definition for defining IOCTL and FSCTL function control codes.  Note
// that function codes 0-2047 are reserved for Microsoft Corporation, and
// 2048-4095 are reserved for customers.
//

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) ( \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

//
// Define the method codes for how buffers are passed for I/O and FS controls
//

#define METHOD_BUFFERED 0
#define METHOD_IN_DIRECT 1
#define METHOD_OUT_DIRECT 2

#ifndef METHOD_NEITHER
#define METHOD_NEITHER 3
#endif

//
// Define some easier to comprehend aliases:
//   METHOD_DIRECT_TO_HARDWARE (writes, aka METHOD_IN_DIRECT)
//   METHOD_DIRECT_FROM_HARDWARE (reads, aka METHOD_OUT_DIRECT)
//

#define METHOD_DIRECT_TO_HARDWARE METHOD_IN_DIRECT
#define METHOD_DIRECT_FROM_HARDWARE METHOD_OUT_DIRECT

#pragma endregion
};
#pragma endregion

#pragma region ntifs.h(public DDK)

extern "C" {
#define RtlOffsetToPointer(B, O) ((PCHAR)(((PCHAR)(B)) + ((ULONG_PTR)(O))))

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtQueryVolumeInformationFile(
            _In_ HANDLE FileHandle,
            _Out_ PIO_STATUS_BLOCK IoStatusBlock,
            _Out_writes_bytes_(Length) PVOID FsInformation,
            _In_ ULONG Length,
            _In_ FS_INFORMATION_CLASS FsInformationClass);
};

#pragma endregion

#endif // _DDK_INCLUDED
