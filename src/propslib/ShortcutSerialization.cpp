// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <intsafe.h>
#include <propvarutil.h>
#include <shlwapi.h>

#include "ShortcutSerialization.hpp"

#pragma hdrstop

void ShortcutSerialization::s_InitPropVarFromBool(_In_ BOOL fVal, _Out_ PROPVARIANT* ppropvar)
{
    ppropvar->vt = VT_BOOL;
    ppropvar->boolVal = fVal ? VARIANT_TRUE : VARIANT_FALSE;
}

void ShortcutSerialization::s_InitPropVarFromByte(_In_ BYTE bVal, _Out_ PROPVARIANT* ppropvar)
{
    ppropvar->vt = VT_I2;
    ppropvar->iVal = bVal;
}

void ShortcutSerialization::s_InitPropVarFromDword(_In_ DWORD dwVal, _Out_ PROPVARIANT* ppropvar)
{
    // A DWORD is a 4-byte unsigned int value, so use the ui4 member.
    // DO NOT use VT_UINT, that doesn't work with PROPVARIANTs.
    // see: https://docs.microsoft.com/en-us/windows/desktop/api/wtypes/ne-wtypes-varenum
    // also MSFT:18312914
    ppropvar->vt = VT_UI4;
    ppropvar->ulVal = dwVal;
}

void ShortcutSerialization::s_SetLinkPropertyBoolValue(_In_ IPropertyStore* pps,
                                                       _In_ REFPROPERTYKEY refPropKey,
                                                       const BOOL fVal)
{
    PROPVARIANT propvarBool;
    s_InitPropVarFromBool(fVal, &propvarBool);
    pps->SetValue(refPropKey, propvarBool);
    PropVariantClear(&propvarBool);
}

void ShortcutSerialization::s_SetLinkPropertyByteValue(_In_ IPropertyStore* pps,
                                                       _In_ REFPROPERTYKEY refPropKey,
                                                       const BYTE bVal)
{
    PROPVARIANT propvarByte;
    s_InitPropVarFromByte(bVal, &propvarByte);
    pps->SetValue(refPropKey, propvarByte);
    PropVariantClear(&propvarByte);
}

void ShortcutSerialization::s_SetLinkPropertyDwordValue(_Inout_ IPropertyStore* pps,
                                                        _In_ REFPROPERTYKEY refPropKey,
                                                        const DWORD dwVal)
{
    PROPVARIANT propvarDword;
    s_InitPropVarFromDword(dwVal, &propvarDword);
    pps->SetValue(refPropKey, propvarDword);
    PropVariantClear(&propvarDword);
}

[[nodiscard]] HRESULT ShortcutSerialization::s_GetPropertyBoolValue(_In_ IPropertyStore* const pPropStore,
                                                                    _In_ REFPROPERTYKEY refPropKey,
                                                                    _Out_ BOOL* const pfValue)
{
    PROPVARIANT propvar;
    HRESULT hr = pPropStore->GetValue(refPropKey, &propvar);
    // Only retrieve the value if we actually found one. If the link didn't have
    //      the property (eg a new property was added_, then ignore it.
    if (SUCCEEDED(hr) && propvar.vt != VT_EMPTY)
    {
        hr = PropVariantToBoolean(propvar, pfValue);
    }

    return hr;
}

[[nodiscard]] HRESULT ShortcutSerialization::s_GetPropertyByteValue(_In_ IPropertyStore* const pPropStore,
                                                                    _In_ REFPROPERTYKEY refPropKey,
                                                                    _Out_ BYTE* const pbValue)
{
    PROPVARIANT propvar;
    HRESULT hr = pPropStore->GetValue(refPropKey, &propvar);
    // Only retrieve the value if we actually found one. If the link didn't have
    //      the property (eg a new property was added_, then ignore it.
    if (SUCCEEDED(hr) && propvar.vt != VT_EMPTY)
    {
        SHORT sValue;
        hr = PropVariantToInt16(propvar, &sValue);
        if (SUCCEEDED(hr))
        {
            hr = (sValue >= 0 && sValue <= BYTE_MAX) ? S_OK : E_INVALIDARG;
            if (SUCCEEDED(hr))
            {
                *pbValue = (BYTE)sValue;
            }
        }
    }

    return hr;
}

[[nodiscard]] HRESULT ShortcutSerialization::s_GetPropertyDwordValue(_Inout_ IPropertyStore* const pPropStore,
                                                                     _In_ REFPROPERTYKEY refPropKey,
                                                                     _Out_ DWORD* const pdwValue)
{
    PROPVARIANT propvar;
    HRESULT hr = pPropStore->GetValue(refPropKey, &propvar);
    // Only retrieve the value if we actually found one. If the link didn't have
    //      the property (eg a new property was added_, then ignore it.
    if (SUCCEEDED(hr) && propvar.vt != VT_EMPTY)
    {
        DWORD dwValue;
        hr = PropVariantToUInt32(propvar, &dwValue);
        if (SUCCEEDED(hr))
        {
            *pdwValue = dwValue;
        }
    }

    return hr;
}

[[nodiscard]] HRESULT ShortcutSerialization::s_PopulateV1Properties(_In_ IShellLink* const pslConsole,
                                                                    _In_ PCONSOLE_STATE_INFO pStateInfo)
{
    IShellLinkDataList* pConsoleLnkDataList;
    HRESULT hr = pslConsole->QueryInterface(IID_PPV_ARGS(&pConsoleLnkDataList));
    if (SUCCEEDED(hr))
    {
        // get/apply standard console properties
        NT_CONSOLE_PROPS* pNtConsoleProps = nullptr;
        hr = pConsoleLnkDataList->CopyDataBlock(NT_CONSOLE_PROPS_SIG, reinterpret_cast<void**>(&pNtConsoleProps));
        if (SUCCEEDED(hr))
        {
            pStateInfo->ScreenAttributes = pNtConsoleProps->wFillAttribute;
            pStateInfo->PopupAttributes = pNtConsoleProps->wPopupFillAttribute;
            pStateInfo->ScreenBufferSize = pNtConsoleProps->dwScreenBufferSize;
            pStateInfo->WindowSize = pNtConsoleProps->dwWindowSize;
            pStateInfo->WindowPosX = pNtConsoleProps->dwWindowOrigin.X;
            pStateInfo->WindowPosY = pNtConsoleProps->dwWindowOrigin.Y;
            pStateInfo->FontSize = pNtConsoleProps->dwFontSize;
            pStateInfo->FontFamily = pNtConsoleProps->uFontFamily;
            pStateInfo->FontWeight = pNtConsoleProps->uFontWeight;
            StringCchCopyW(pStateInfo->FaceName, ARRAYSIZE(pStateInfo->FaceName), pNtConsoleProps->FaceName);
            pStateInfo->CursorSize = pNtConsoleProps->uCursorSize;
            pStateInfo->FullScreen = pNtConsoleProps->bFullScreen;
            pStateInfo->QuickEdit = pNtConsoleProps->bQuickEdit;
            pStateInfo->InsertMode = pNtConsoleProps->bInsertMode;
            pStateInfo->AutoPosition = pNtConsoleProps->bAutoPosition;
            pStateInfo->HistoryBufferSize = pNtConsoleProps->uHistoryBufferSize;
            pStateInfo->NumberOfHistoryBuffers = pNtConsoleProps->uNumberOfHistoryBuffers;
            pStateInfo->HistoryNoDup = pNtConsoleProps->bHistoryNoDup;
            CopyMemory(pStateInfo->ColorTable, pNtConsoleProps->ColorTable, sizeof(pStateInfo->ColorTable));

            LocalFree(pNtConsoleProps);
        }

        // get/apply international console properties
        if (SUCCEEDED(hr))
        {
            NT_FE_CONSOLE_PROPS* pNtFEConsoleProps;
            if (SUCCEEDED(pConsoleLnkDataList->CopyDataBlock(NT_FE_CONSOLE_PROPS_SIG, reinterpret_cast<void**>(&pNtFEConsoleProps))))
            {
                pNtFEConsoleProps->uCodePage = pStateInfo->CodePage;
                LocalFree(pNtFEConsoleProps);
            }
        }

        pConsoleLnkDataList->Release();
    }

    return hr;
}

[[nodiscard]] HRESULT ShortcutSerialization::s_PopulateV2Properties(_In_ IShellLink* const pslConsole,
                                                                    _In_ PCONSOLE_STATE_INFO pStateInfo)
{
    IPropertyStore* pPropStoreLnk;
    HRESULT hr = pslConsole->QueryInterface(IID_PPV_ARGS(&pPropStoreLnk));
    if (SUCCEEDED(hr))
    {
        hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_WrapText, &pStateInfo->fWrapText);
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_FilterOnPaste, &pStateInfo->fFilterOnPaste);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_CtrlKeyShortcutsDisabled, &pStateInfo->fCtrlKeyShortcutsDisabled);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_LineSelection, &pStateInfo->fLineSelection);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyByteValue(pPropStoreLnk, PKEY_Console_WindowTransparency, &pStateInfo->bWindowTransparency);
        }
        if (SUCCEEDED(hr))
        {
            DWORD placeholder = 0;
            hr = s_GetPropertyDwordValue(pPropStoreLnk, PKEY_Console_CursorType, &placeholder);
            if (SUCCEEDED(hr))
            {
                pStateInfo->CursorType = (unsigned int)placeholder;
            }
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyDwordValue(pPropStoreLnk, PKEY_Console_CursorColor, &pStateInfo->CursorColor);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_InterceptCopyPaste, &pStateInfo->InterceptCopyPaste);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyDwordValue(pPropStoreLnk, PKEY_Console_DefaultForeground, &pStateInfo->DefaultForeground);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyDwordValue(pPropStoreLnk, PKEY_Console_DefaultBackground, &pStateInfo->DefaultBackground);
        }
        if (SUCCEEDED(hr))
        {
            hr = s_GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_TerminalScrolling, &pStateInfo->TerminalScrolling);
        }

        pPropStoreLnk->Release();
    }

    return hr;
}

// Given a shortcut filename, determine what title we should use. Under normal circumstances, we rely on the shell to
// provide the correct title. However, if that fails, we'll just use the shortcut filename minus the extension.
void ShortcutSerialization::s_GetLinkTitle(_In_ PCWSTR pwszShortcutFilename,
                                           _Out_writes_(cchShortcutTitle) PWSTR pwszShortcutTitle,
                                           const size_t cchShortcutTitle)
{
    NTSTATUS Status = (cchShortcutTitle > 0) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER_2;
    if (NT_SUCCESS(Status))
    {
        pwszShortcutTitle[0] = L'\0';

        WCHAR szTemp[MAX_PATH];
        Status = StringCchCopyW(szTemp, ARRAYSIZE(szTemp), pwszShortcutFilename);
        if (NT_SUCCESS(Status))
        {
            // Now load the localized title for the shortcut
            IShellItem* psi;
            HRESULT hrShellItem = SHCreateItemFromParsingName(pwszShortcutFilename, nullptr, IID_PPV_ARGS(&psi));
            if (SUCCEEDED(hrShellItem))
            {
                PWSTR pwszShortcutDisplayName;
                hrShellItem = psi->GetDisplayName(SIGDN_NORMALDISPLAY, &pwszShortcutDisplayName);
                if (SUCCEEDED(hrShellItem))
                {
                    Status = StringCchCopyW(pwszShortcutTitle, cchShortcutTitle, pwszShortcutDisplayName);
                    CoTaskMemFree(pwszShortcutDisplayName);
                }

                psi->Release();
            }
        }

        if (!NT_SUCCESS(Status))
        {
            // default to an extension-free version of the filename passed in
            Status = StringCchCopyW(pwszShortcutTitle, cchShortcutTitle, pwszShortcutFilename);
            if (NT_SUCCESS(Status))
            {
                // don't care if we can't remove the extension
                (void)PathCchRemoveExtension(pwszShortcutTitle, cchShortcutTitle);
            }
        }
    }
}

// Given a shortcut filename, retrieve IShellLink and IPersistFile itf ptrs, and ensure that the link is loaded.
[[nodiscard]] HRESULT ShortcutSerialization::s_GetLoadedShellLinkForShortcut(_In_ PCWSTR pwszShortcutFileName,
                                                                             const DWORD dwMode,
                                                                             _COM_Outptr_ IShellLink** ppsl,
                                                                             _COM_Outptr_ IPersistFile** ppPf)
{
    *ppsl = nullptr;
    *ppPf = nullptr;
    IShellLink* psl;
    HRESULT hr = SHCoCreateInstance(NULL, &CLSID_ShellLink, NULL, IID_PPV_ARGS(&psl));
    if (SUCCEEDED(hr))
    {
        IPersistFile* pPf;
        hr = psl->QueryInterface(IID_PPV_ARGS(&pPf));
        if (SUCCEEDED(hr))
        {
            hr = pPf->Load(pwszShortcutFileName, dwMode);
            if (SUCCEEDED(hr))
            {
                hr = psl->QueryInterface(IID_PPV_ARGS(ppsl));
                if (SUCCEEDED(hr))
                {
                    hr = pPf->QueryInterface(IID_PPV_ARGS(ppPf));
                    if (FAILED(hr))
                    {
                        (*ppsl)->Release();
                        *ppsl = nullptr;
                    }
                }
            }
            pPf->Release();
        }
        psl->Release();
    }

    return hr;
}

// Retrieves console-only properties from the shortcut file specified in pStateInfo. Used by the console properties sheet.
[[nodiscard]] NTSTATUS ShortcutSerialization::s_GetLinkConsoleProperties(_Inout_ PCONSOLE_STATE_INFO pStateInfo)
{
    IShellLink* psl;
    IPersistFile* ppf;
    HRESULT hr = s_GetLoadedShellLinkForShortcut(pStateInfo->LinkTitle, STGM_READ, &psl, &ppf);
    if (SUCCEEDED(hr))
    {
        hr = s_PopulateV1Properties(psl, pStateInfo);
        if (SUCCEEDED(hr))
        {
            hr = s_PopulateV2Properties(psl, pStateInfo);
        }

        ppf->Release();
        psl->Release();
    }
    return (SUCCEEDED(hr)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

// Retrieves all shortcut properties from the file specified in pStateInfo. Used by conhostv2.dll
[[nodiscard]] NTSTATUS ShortcutSerialization::s_GetLinkValues(_Inout_ PCONSOLE_STATE_INFO pStateInfo,
                                                              _Out_ BOOL* const pfReadConsoleProperties,
                                                              _Out_writes_opt_(cchShortcutTitle) PWSTR pwszShortcutTitle,
                                                              const size_t cchShortcutTitle,
                                                              _Out_writes_opt_(cchIconLocation) PWSTR pwszIconLocation,
                                                              const size_t cchIconLocation,
                                                              _Out_opt_ int* const piIcon,
                                                              _Out_opt_ int* const piShowCmd,
                                                              _Out_opt_ WORD* const pwHotKey)
{
    *pfReadConsoleProperties = false;

    if (pwszShortcutTitle && cchShortcutTitle > 0)
    {
        pwszShortcutTitle[0] = L'\0';
    }

    if (pwszIconLocation && cchIconLocation > 0)
    {
        pwszIconLocation[0] = L'\0';
    }

    IShellLink* psl;
    IPersistFile* ppf;
    HRESULT hr = s_GetLoadedShellLinkForShortcut(pStateInfo->LinkTitle, STGM_READ, &psl, &ppf);
    if (SUCCEEDED(hr))
    {
        // first, load non-console-specific shortcut properties, if requested
        if (pwszShortcutTitle)
        {
            // note: the "LinkTitle" member actually holds the filename of the shortcut, it's just poorly named.
            s_GetLinkTitle(pStateInfo->LinkTitle, pwszShortcutTitle, cchShortcutTitle);
        }

        if (pwszIconLocation && piIcon)
        {
            hr = psl->GetIconLocation(pwszIconLocation, static_cast<int>(cchIconLocation), piIcon);
        }

        if (SUCCEEDED(hr) && piShowCmd)
        {
            hr = psl->GetShowCmd(piShowCmd);
        }

        if (SUCCEEDED(hr) && pwHotKey)
        {
            hr = psl->GetHotkey(pwHotKey);
        }

        if (SUCCEEDED(hr))
        {
            // now load console-specific shortcut properties. note that we don't want to propagate errors out
            // here, since we've historically had two outcomes from this function -- we read generic shortcut
            // properties (above), and then more specific ones. if the specific ones fail, we still treat it
            // like a success so that we can continue to load.
            HRESULT hrProps = s_PopulateV1Properties(psl, pStateInfo);
            if (SUCCEEDED(hrProps))
            {
                *pfReadConsoleProperties = true;
                LOG_IF_FAILED(s_PopulateV2Properties(psl, pStateInfo));
            }
        }
        ppf->Release();
        psl->Release();
    }

    return (SUCCEEDED(hr)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

// Function Description:
// - Writes the console properties out to the link it was opened from.
// Arguments:
// - pStateInfo: pointer to structure containing information
// - writeTerminalSettings: If true, persist the "Terminal" properties only
//   present in the v2 console. This should be false if called from a v11
//   console. See GH#2319
// Return Value:
// - A status code if something failed or S_OK
[[nodiscard]] NTSTATUS ShortcutSerialization::s_SetLinkValues(_In_ PCONSOLE_STATE_INFO pStateInfo,
                                                              const BOOL fEastAsianSystem,
                                                              const BOOL fForceV2,
                                                              const bool writeTerminalSettings)
{
    IShellLinkW* psl;
    IPersistFile* ppf;
    HRESULT hr = s_GetLoadedShellLinkForShortcut(pStateInfo->LinkTitle, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &psl, &ppf);
    if (SUCCEEDED(hr))
    {
        IShellLinkDataList* psldl;
        hr = psl->QueryInterface(IID_PPV_ARGS(&psldl));
        if (SUCCEEDED(hr))
        {
            // Now the link is loaded, generate new console settings section to replace the one in the link.
            NT_CONSOLE_PROPS props;
            ((LPDBLIST)&props)->cbSize = sizeof(props);
            ((LPDBLIST)&props)->dwSignature = NT_CONSOLE_PROPS_SIG;
            props.wFillAttribute = pStateInfo->ScreenAttributes;
            props.wPopupFillAttribute = pStateInfo->PopupAttributes;
            props.dwScreenBufferSize = pStateInfo->ScreenBufferSize;
            props.dwWindowSize = pStateInfo->WindowSize;
            props.dwWindowOrigin.X = (SHORT)pStateInfo->WindowPosX;
            props.dwWindowOrigin.Y = (SHORT)pStateInfo->WindowPosY;
            props.nFont = 0;
            props.nInputBufferSize = 0;
            props.dwFontSize = pStateInfo->FontSize;
            props.uFontFamily = pStateInfo->FontFamily;
            props.uFontWeight = pStateInfo->FontWeight;
            CopyMemory(props.FaceName, pStateInfo->FaceName, sizeof(props.FaceName));
            props.uCursorSize = pStateInfo->CursorSize;
            props.bFullScreen = pStateInfo->FullScreen;
            props.bQuickEdit = pStateInfo->QuickEdit;
            props.bInsertMode = pStateInfo->InsertMode;
            props.bAutoPosition = pStateInfo->AutoPosition;
            props.uHistoryBufferSize = pStateInfo->HistoryBufferSize;
            props.uNumberOfHistoryBuffers = pStateInfo->NumberOfHistoryBuffers;
            props.bHistoryNoDup = pStateInfo->HistoryNoDup;
            CopyMemory(props.ColorTable, pStateInfo->ColorTable, sizeof(props.ColorTable));

            // Store the changes back into the link...
            hr = psldl->RemoveDataBlock(NT_CONSOLE_PROPS_SIG);
            if (SUCCEEDED(hr))
            {
                hr = psldl->AddDataBlock((LPVOID)&props);
            }

            if (SUCCEEDED(hr) && fEastAsianSystem)
            {
                NT_FE_CONSOLE_PROPS fe_props;
                ((LPDBLIST)&fe_props)->cbSize = sizeof(fe_props);
                ((LPDBLIST)&fe_props)->dwSignature = NT_FE_CONSOLE_PROPS_SIG;
                fe_props.uCodePage = pStateInfo->CodePage;

                hr = psldl->RemoveDataBlock(NT_FE_CONSOLE_PROPS_SIG);
                if (SUCCEEDED(hr))
                {
                    hr = psldl->AddDataBlock((LPVOID)&fe_props);
                }
            }

            if (SUCCEEDED(hr))
            {
                IPropertyStore* pps;
                hr = psl->QueryInterface(IID_IPropertyStore, reinterpret_cast<void**>(&pps));
                if (SUCCEEDED(hr))
                {
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_ForceV2, fForceV2);
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_WrapText, pStateInfo->fWrapText);
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_FilterOnPaste, pStateInfo->fFilterOnPaste);
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_CtrlKeyShortcutsDisabled, pStateInfo->fCtrlKeyShortcutsDisabled);
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_LineSelection, pStateInfo->fLineSelection);
                    s_SetLinkPropertyByteValue(pps, PKEY_Console_WindowTransparency, pStateInfo->bWindowTransparency);
                    s_SetLinkPropertyBoolValue(pps, PKEY_Console_InterceptCopyPaste, pStateInfo->InterceptCopyPaste);

                    // Only save the "Terminal" settings if we launched as a v2
                    // propsheet. The v1 console doesn't know anything about
                    // these settings, and their value will be incorrectly
                    // zero'd if we save in this state.
                    // See microsoft/terminal#2319 for more details.
                    if (writeTerminalSettings)
                    {
                        s_SetLinkPropertyDwordValue(pps, PKEY_Console_CursorType, pStateInfo->CursorType);
                        s_SetLinkPropertyDwordValue(pps, PKEY_Console_CursorColor, pStateInfo->CursorColor);
                        s_SetLinkPropertyDwordValue(pps, PKEY_Console_DefaultForeground, pStateInfo->DefaultForeground);
                        s_SetLinkPropertyDwordValue(pps, PKEY_Console_DefaultBackground, pStateInfo->DefaultBackground);
                        s_SetLinkPropertyBoolValue(pps, PKEY_Console_TerminalScrolling, pStateInfo->TerminalScrolling);
                    }
                    hr = pps->Commit();
                    pps->Release();
                }
            }

            psldl->Release();
        }

        if (SUCCEEDED(hr))
        {
            // Only persist changes if we've successfully made them.
            hr = ppf->Save(NULL, TRUE);
        }

        ppf->Release();
        psl->Release();
    }

    return (SUCCEEDED(hr)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}
