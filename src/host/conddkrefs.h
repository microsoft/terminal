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

#include <winternl.h>

extern "C" {

#pragma region wdm.h(public DDK)

// UNICODE_STRING

extern "C++" {
char _RTL_CONSTANT_STRING_type_check(const char* s);
char _RTL_CONSTANT_STRING_type_check(const WCHAR* s);

template<size_t N>
class _RTL_CONSTANT_STRING_remove_const_template_class;

template<>
class _RTL_CONSTANT_STRING_remove_const_template_class<sizeof(char)>
{
public:
    typedef char T;
};

template<>
class _RTL_CONSTANT_STRING_remove_const_template_class<sizeof(WCHAR)>
{
public:
    typedef WCHAR T;
};

#define _RTL_CONSTANT_STRING_remove_const_macro(s) \
    (const_cast<_RTL_CONSTANT_STRING_remove_const_template_class<sizeof((s)[0])>::T*>(s))

#define RTL_CONSTANT_STRING(s)                                  \
    {                                                           \
        sizeof(s) - sizeof((s)[0]),                             \
        sizeof(s) / sizeof(_RTL_CONSTANT_STRING_type_check(s)), \
        _RTL_CONSTANT_STRING_remove_const_macro(s)              \
    }
}

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

#pragma endregion

#pragma region ntifs.h(public DDK)

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtQueryVolumeInformationFile(
            _In_ HANDLE FileHandle,
            _Out_ PIO_STATUS_BLOCK IoStatusBlock,
            _Out_writes_bytes_(Length) PVOID FsInformation,
            _In_ ULONG Length,
            _In_ FS_INFORMATION_CLASS FsInformationClass);

#pragma endregion

#pragma region ntddk.h/ntifs.h(public DDK) - sections, mutants, object duplication
// Used to back CONSOLE_GRAPHICS_BUFFER screen buffers with a section that's
// mapped both into conhost and directly into the requesting client's process.

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtCreateSection(
            _Out_ PHANDLE SectionHandle,
            _In_ ACCESS_MASK DesiredAccess,
            _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
            _In_opt_ PLARGE_INTEGER MaximumSize,
            _In_ ULONG SectionPageProtection,
            _In_ ULONG AllocationAttributes,
            _In_opt_ HANDLE FileHandle);

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtMapViewOfSection(
            _In_ HANDLE SectionHandle,
            _In_ HANDLE ProcessHandle,
            _Inout_ PVOID* BaseAddress,
            _In_ ULONG_PTR ZeroBits,
            _In_ SIZE_T CommitSize,
            _Inout_opt_ PLARGE_INTEGER SectionOffset,
            _Inout_ PSIZE_T ViewSize,
            // NOTE: this is really the SECTION_INHERIT enum (ViewShare=1, ViewUnmap=2),
            // declared as a plain ULONG here because SECTION_INHERIT itself lives in
            // <winnt.h>, which isn't guaranteed to be pulled in by every project that
            // includes this header (e.g. winconptylib) - see the file banner comment.
            _In_ ULONG InheritDisposition,
            _In_ ULONG AllocationType,
            _In_ ULONG Win32Protect);

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtUnmapViewOfSection(
            _In_ HANDLE ProcessHandle,
            _In_opt_ PVOID BaseAddress);

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtDuplicateObject(
            _In_ HANDLE SourceProcessHandle,
            _In_ HANDLE SourceHandle,
            _In_opt_ HANDLE TargetProcessHandle,
            _Out_opt_ PHANDLE TargetHandle,
            _In_ ACCESS_MASK DesiredAccess,
            _In_ ULONG HandleAttributes,
            _In_ ULONG Options);

// The real SECTION_INHERIT::ViewUnmap value (see NtMapViewOfSection's
// InheritDisposition parameter above) - defined here rather than relying on
// <winnt.h>'s SECTION_INHERIT enum being visible, for the same reason that
// parameter is typed ULONG instead of SECTION_INHERIT.
#ifndef NT_VIEW_UNMAP
#define NT_VIEW_UNMAP 2
#endif

__kernel_entry NTSYSCALLAPI
    NTSTATUS
        NTAPI
        NtCreateMutant(
            _Out_ PHANDLE MutantHandle,
            _In_ ACCESS_MASK DesiredAccess,
            _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
            _In_ BOOLEAN InitialOwner);

#pragma endregion

// InteractivityOneCore depends on this private function. The IsPresent checks
// are automatically generated by forwarder.template and aren't part of the DDK.
// I've placed it here because I couldn't come up with a better place.
BOOL IsGetSystemMetricsPresent();
}
#endif // _DDK_INCLUDED
