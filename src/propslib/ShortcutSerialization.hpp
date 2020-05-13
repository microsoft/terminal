/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ShortcutSerialization.hpp

Abstract:
- This module is used for writing console properties to the link associated
    with a particular console title.

Author(s):
- Michael Niksa (MiNiksa) 23-Jul-2014
- Paul Campbell (PaulCam) 23-Jul-2014
- Mike Griese   (MiGrie)  04-Aug-2015

Revision History:
- From components of srvinit.c
- From miniksa, paulcam's Registry.cpp
--*/

#pragma once

class ShortcutSerialization
{
public:
    [[nodiscard]] static NTSTATUS s_SetLinkValues(_In_ PCONSOLE_STATE_INFO pStateInfo, const BOOL fEastAsianSystem, const BOOL fForceV2, const bool writeTerminalSettings);
    [[nodiscard]] static NTSTATUS s_GetLinkConsoleProperties(_Inout_ PCONSOLE_STATE_INFO pStateInfo);
    [[nodiscard]] static NTSTATUS s_GetLinkValues(_Inout_ PCONSOLE_STATE_INFO pStateInfo,
                                                  _Out_ BOOL* const pfReadConsoleProperties,
                                                  _Out_writes_opt_(cchShortcutTitle) PWSTR pwszShortcutTitle,
                                                  const size_t cchShortcutTitle,
                                                  _Out_writes_opt_(cchIconLocation) PWSTR pwszIconLocation,
                                                  const size_t cchIconLocation,
                                                  _Out_opt_ int* const piIcon,
                                                  _Out_opt_ int* const piShowCmd,
                                                  _Out_opt_ WORD* const pwHotKey);

private:
    static void s_InitPropVarFromBool(_In_ BOOL fVal, _Out_ PROPVARIANT* ppropvar);
    static void s_InitPropVarFromByte(_In_ BYTE bVal, _Out_ PROPVARIANT* ppropvar);
    static void s_InitPropVarFromDword(_In_ DWORD dwVal, _Out_ PROPVARIANT* ppropvar);

    static void s_SetLinkPropertyBoolValue(_In_ IPropertyStore* pps, _In_ REFPROPERTYKEY refPropKey, const BOOL fVal);
    static void s_SetLinkPropertyByteValue(_In_ IPropertyStore* pps, _In_ REFPROPERTYKEY refPropKey, const BYTE bVal);
    static void s_SetLinkPropertyDwordValue(_In_ IPropertyStore* pps, _In_ REFPROPERTYKEY refPropKey, const DWORD dwVal);

    [[nodiscard]] static HRESULT s_GetPropertyBoolValue(_In_ IPropertyStore* const pPropStore,
                                                        _In_ REFPROPERTYKEY refPropKey,
                                                        _Out_ BOOL* const pfValue);
    [[nodiscard]] static HRESULT s_GetPropertyByteValue(_In_ IPropertyStore* const pPropStore,
                                                        _In_ REFPROPERTYKEY refPropKey,
                                                        _Out_ BYTE* const pbValue);
    [[nodiscard]] static HRESULT s_GetPropertyDwordValue(_In_ IPropertyStore* const pPropStore,
                                                         _In_ REFPROPERTYKEY refPropKey,
                                                         _Out_ DWORD* const pdwValue);

    [[nodiscard]] static HRESULT s_PopulateV1Properties(_In_ IShellLink* const pslConsole, _In_ PCONSOLE_STATE_INFO pStateInfo);
    [[nodiscard]] static HRESULT s_PopulateV2Properties(_In_ IShellLink* const pslConsole, _In_ PCONSOLE_STATE_INFO pStateInfo);

    static void s_GetLinkTitle(_In_ PCWSTR pwszShortcutFilename, _Out_writes_(cchShortcutTitle) PWSTR pwszShortcutTitle, const size_t cchShortcutTitle);
    [[nodiscard]] static HRESULT s_GetLoadedShellLinkForShortcut(_In_ PCWSTR pwszShortcutFileName,
                                                                 const DWORD dwMode,
                                                                 _COM_Outptr_ IShellLink** ppsl,
                                                                 _COM_Outptr_ IPersistFile** ppPf);
};
