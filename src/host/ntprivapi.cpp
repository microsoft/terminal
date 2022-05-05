// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "NtPrivApi.hpp"

[[nodiscard]] NTSTATUS NtPrivApi::s_GetProcessParentId(_Inout_ PULONG ProcessId)
{
    // TODO: Get Parent current not really available without winternl + NtQueryInformationProcess. http://osgvsowi/8394495
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);

    CLIENT_ID ClientId;
    ClientId.UniqueProcess = UlongToHandle(*ProcessId);
    ClientId.UniqueThread = nullptr;

    HANDLE ProcessHandle;
    auto Status = s_NtOpenProcess(&ProcessHandle, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &ClientId);

    PROCESS_BASIC_INFORMATION BasicInfo = { 0 };
    if (NT_SUCCESS(Status))
    {
        Status = s_NtQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), nullptr);
        LOG_IF_FAILED(s_NtClose(ProcessHandle));
    }

    if (!NT_SUCCESS(Status))
    {
        *ProcessId = 0;
        return Status;
    }

    *ProcessId = (ULONG)BasicInfo.InheritedFromUniqueProcessId;
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS NtPrivApi::s_NtOpenProcess(_Out_ PHANDLE ProcessHandle,
                                                  _In_ ACCESS_MASK DesiredAccess,
                                                  _In_ POBJECT_ATTRIBUTES ObjectAttributes,
                                                  _In_opt_ PCLIENT_ID ClientId)
{
    auto hNtDll = _Instance()._hNtDll;

    if (hNtDll != nullptr)
    {
        typedef NTSTATUS (*PfnNtOpenProcess)(HANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);

        static auto pfn = (PfnNtOpenProcess)GetProcAddress(hNtDll, "NtOpenProcess");

        if (pfn != nullptr)
        {
            return pfn(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
        }
    }

    return STATUS_UNSUCCESSFUL;
}

[[nodiscard]] NTSTATUS NtPrivApi::s_NtQueryInformationProcess(_In_ HANDLE ProcessHandle,
                                                              _In_ PROCESSINFOCLASS ProcessInformationClass,
                                                              _Out_ PVOID ProcessInformation,
                                                              _In_ ULONG ProcessInformationLength,
                                                              _Out_opt_ PULONG ReturnLength)
{
    auto hNtDll = _Instance()._hNtDll;

    if (hNtDll != nullptr)
    {
        typedef NTSTATUS (*PfnNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);

        static auto pfn = (PfnNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");

        if (pfn != nullptr)
        {
            return pfn(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
        }
    }

    return STATUS_UNSUCCESSFUL;
}

[[nodiscard]] NTSTATUS NtPrivApi::s_NtClose(_In_ HANDLE Handle)
{
    auto hNtDll = _Instance()._hNtDll;

    if (hNtDll != nullptr)
    {
        typedef NTSTATUS (*PfnNtClose)(HANDLE Handle);

        static auto pfn = (PfnNtClose)GetProcAddress(hNtDll, "NtClose");

        if (pfn != nullptr)
        {
            return pfn(Handle);
        }
    }

    return STATUS_UNSUCCESSFUL;
}

NtPrivApi::NtPrivApi()
{
    // NOTE: Use LoadLibraryExW with LOAD_LIBRARY_SEARCH_SYSTEM32 flag below to avoid unneeded directory traversal.
    //       This has triggered CPG boot IO warnings in the past.
    _hNtDll = LoadLibraryExW(L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

NtPrivApi::~NtPrivApi()
{
    if (_hNtDll != nullptr)
    {
        FreeLibrary(_hNtDll);
        _hNtDll = nullptr;
    }
}

NtPrivApi& NtPrivApi::_Instance()
{
    static NtPrivApi ntapi;
    return ntapi;
}
