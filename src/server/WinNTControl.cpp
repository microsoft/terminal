// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WinNTControl.h"

// Routine Description:
// - Creates an instance of the NTDLL method-invoking class.
// - This class helps maintain a loose coupling on NTDLL without reliance on the driver kit headers/libs.
WinNTControl::WinNTControl() :
    // NOTE: Use LoadLibraryExW with LOAD_LIBRARY_SEARCH_SYSTEM32 flag below to avoid unneeded directory traversal.
    //       This has triggered CPG boot IO warnings in the past.
    _NtDllDll(THROW_LAST_ERROR_IF_NULL(LoadLibraryExW(L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))),
    _NtOpenFile(reinterpret_cast<PfnNtOpenFile>(THROW_LAST_ERROR_IF_NULL(GetProcAddress(_NtDllDll.get(), "NtOpenFile"))))
{
}

// Routine Description:
// - Destructs an instance of the NTDLL method-invoking class.
WinNTControl::~WinNTControl()
{
}

// Routine Description:
// - Provides the singleton pattern for WinNT control. Stores the single instance and returns it.
// Arguments:
// - <none>
// Return Value:
// - Reference to the single instance of NTDLL.dll wrapped methods.
WinNTControl& WinNTControl::GetInstance()
{
    static WinNTControl Instance;
    return Instance;
}

// Routine Description:
// - Provides access to the NtOpenFile method documented at:
//   https://msdn.microsoft.com/en-us/library/bb432381(v=vs.85).aspx
// Arguments:
// - See definitions at MSDN
// Return Value:
// - See definitions at MSDN
[[nodiscard]] NTSTATUS WinNTControl::NtOpenFile(_Out_ PHANDLE FileHandle,
                                                _In_ ACCESS_MASK DesiredAccess,
                                                _In_ POBJECT_ATTRIBUTES ObjectAttributes,
                                                _Out_ PIO_STATUS_BLOCK IoStatusBlock,
                                                _In_ ULONG ShareAccess,
                                                _In_ ULONG OpenOptions)
{
    try
    {
        return GetInstance()._NtOpenFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
    }
    CATCH_RETURN();
}
