/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RegistrySerialization.hpp

Abstract:
- This module is used for reading/writing registry operations

Author(s):
- Michael Niksa (MiNiksa) 23-Jul-2014
- Paul Campbell (PaulCam) 23-Jul-2014
- Mike Griese   (MiGrie)  04-Aug-2015

Revision History:
- From components of srvinit.c
- From miniksa, paulcam's Registry.cpp
--*/

#pragma once

class Settings;

class RegistrySerialization
{
public:
    // The following registry methods remain public for DBCS and EUDC lookups.
    [[nodiscard]] static NTSTATUS s_OpenKey(_In_opt_ HKEY const hKey, _In_ PCWSTR const pwszSubKey, _Out_ HKEY* const phResult);

    [[nodiscard]] static NTSTATUS s_QueryValue(const HKEY hKey,
                                               _In_ PCWSTR const pwszValueName,
                                               const DWORD cbValueLength,
                                               const DWORD regType,
                                               _Out_writes_bytes_(cbValueLength) BYTE* const pbData,
                                               _Out_opt_ _Out_range_(0, cbValueLength) DWORD* const pcbDataLength);

    [[nodiscard]] static NTSTATUS s_EnumValue(const HKEY hKey,
                                              const DWORD dwIndex,
                                              const DWORD cbValueLength,
                                              _Out_writes_bytes_(cbValueLength) PWSTR const pwszValueName,
                                              const DWORD cbDataLength,
                                              _Out_writes_bytes_(cbDataLength) BYTE* const pbData);

    [[nodiscard]] static NTSTATUS s_OpenConsoleKey(_Out_ HKEY* phCurrentUserKey, _Out_ HKEY* phConsoleKey);

    [[nodiscard]] static NTSTATUS s_CreateKey(const HKEY hKey, _In_ PCWSTR const pwszSubKey, _Out_ HKEY* const phResult);

    [[nodiscard]] static NTSTATUS s_DeleteValue(const HKEY hKey, _In_ PCWSTR const pwszValueName);

    [[nodiscard]] static NTSTATUS s_SetValue(const HKEY hKey,
                                             _In_ PCWSTR const pwszValueName,
                                             const DWORD dwType,
                                             _In_reads_bytes_(cbDataLength) BYTE* const pbData,
                                             const DWORD cbDataLength);

    [[nodiscard]] static NTSTATUS s_UpdateValue(const HKEY hConsoleKey,
                                                const HKEY hKey,
                                                _In_ PCWSTR const pwszValueName,
                                                const DWORD dwType,
                                                _In_reads_bytes_(dwDataLength) BYTE* pbData,
                                                const DWORD dwDataLength);

    [[nodiscard]] static NTSTATUS s_OpenCurrentUserConsoleTitleKey(_In_ PCWSTR const title,
                                                                   _Out_ HKEY* phCurrentUserKey,
                                                                   _Out_ HKEY* phConsoleKey,
                                                                   _Out_ HKEY* phTitleKey);

    enum _RegPropertyType
    {
        Boolean,
        Dword,
        Word,
        Byte,
        Coordinate,
        String,
    };

    static DWORD ToWin32RegistryType(const _RegPropertyType type);

    typedef struct _RegPropertyMap
    {
        _RegPropertyType const propertyType;
        PCWSTR pwszValueName;
        DWORD const dwFieldOffset;
        size_t const cbFieldSize;
        _RegPropertyMap(
            _RegPropertyType const propertyType,
            PCWSTR pwszValueName,
            DWORD const dwFieldOffset,
            size_t const cbFieldSize) :
            propertyType(propertyType),
            pwszValueName(pwszValueName),
            dwFieldOffset(dwFieldOffset),
            cbFieldSize(cbFieldSize){};

        _RegPropertyMap& operator=(const _RegPropertyMap&) { return *this; }
    } RegPropertyMap;

    static const RegPropertyMap s_PropertyMappings[];
    static const size_t s_PropertyMappingsSize;

    static const RegPropertyMap s_GlobalPropMappings[];
    static const size_t s_GlobalPropMappingsSize;

    [[nodiscard]] static NTSTATUS s_LoadRegDword(const HKEY hKey, const _RegPropertyMap* const pPropMap, _In_ Settings* const pSettings);
    [[nodiscard]] static NTSTATUS s_LoadRegString(const HKEY hKey, const _RegPropertyMap* const pPropMap, _In_ Settings* const pSettings);
};
