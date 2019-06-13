// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>

void PrintUsage()
{
    wprintf(L"lnkd usage:\n");
    wprintf(L"\tlnkd <path\\to\\foo.lnk>\n");
}

HRESULT GetPropertyBoolValue(_In_ IPropertyStore* pPropStore, _In_ REFPROPERTYKEY refPropKey, _Out_ BOOL* pfValue)
{
    PROPVARIANT propvar;
    HRESULT hr = pPropStore->GetValue(refPropKey, &propvar);
    if (SUCCEEDED(hr))
    {
        hr = PropVariantToBoolean(propvar, pfValue);
    }

    return hr;
}

HRESULT GetPropertyByteValue(_In_ IPropertyStore* pPropStore, _In_ REFPROPERTYKEY refPropKey, _Out_ BYTE* pbValue)
{
    PROPVARIANT propvar;
    HRESULT hr = pPropStore->GetValue(refPropKey, &propvar);
    if (SUCCEEDED(hr))
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

void DumpV2Properties(_In_ IShellLink* pslConsole)
{
    IPropertyStore* pPropStoreLnk;
    HRESULT hr = pslConsole->QueryInterface(IID_PPV_ARGS(&pPropStoreLnk));
    if (SUCCEEDED(hr))
    {
        wprintf(L"V2 Properties:\n");
        BOOL fForceV2;
        hr = GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_ForceV2, &fForceV2);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_ForceV2: %s\n", (fForceV2) ? L"true" : L"false");
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_ForceV2. (HRESULT: 0x%08x)\n", hr);
        }

        BOOL fWrapText;
        hr = GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_WrapText, &fWrapText);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_WrapText: %s\n", (fWrapText) ? L"true" : L"false");
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_WrapText. (HRESULT: 0x%08x)\n", hr);
        }

        BOOL fFilterOnPaste;
        hr = GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_FilterOnPaste, &fFilterOnPaste);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_FilterOnPaste: %s\n", (fFilterOnPaste) ? L"true" : L"false");
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_FilterOnPaste. (HRESULT: 0x%08x)\n", hr);
        }

        BOOL fCtrlKeyShortcutsDisabled;
        hr = GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_CtrlKeyShortcutsDisabled, &fCtrlKeyShortcutsDisabled);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_CtrlKeyShortcutsDisabled: %s\n", (fCtrlKeyShortcutsDisabled) ? L"true" : L"false");
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_CtrlKeyShortcutsDisabled. (HRESULT: 0x%08x)\n", hr);
        }

        BOOL fLineSelection;
        hr = GetPropertyBoolValue(pPropStoreLnk, PKEY_Console_LineSelection, &fLineSelection);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_LineSelection: %s\n", (fLineSelection) ? L"true" : L"false");
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_LineSelection. (HRESULT: 0x%08x)\n", hr);
        }

        BYTE bWindowTransparency;
        hr = GetPropertyByteValue(pPropStoreLnk, PKEY_Console_WindowTransparency, &bWindowTransparency);
        if (SUCCEEDED(hr))
        {
            wprintf(L"\tPKEY_Console_WindowTransparency: %d\n", bWindowTransparency);
        }
        else
        {
            wprintf(L"ERROR: Unable to retreive value of PKEY_Console_WindowTransparency. (HRESULT: 0x%08x)\n", hr);
        }

        pPropStoreLnk->Release();
    }
}

void DumpCoord(_In_ PCWSTR pszAttrName, const COORD coord)
{
    wprintf(L"\t%s: (%d, %d) (0x%x)\n",
            pszAttrName,
            coord.X,
            coord.Y,
            coord);
}

void DumpBool(_In_ PCWSTR pszAttrName, const BOOL fEnabled)
{
    wprintf(L"\t%s: %s\n", pszAttrName, fEnabled ? L"true" : L"false");
}

HRESULT DumpV1Properties(_In_ IShellLink* pslConsole)
{
    IShellLinkDataList* pConsoleLnkDataList;
    HRESULT hr = pslConsole->QueryInterface(IID_PPV_ARGS(&pConsoleLnkDataList));
    if (SUCCEEDED(hr))
    {
        NT_CONSOLE_PROPS* pNtConsoleProps = nullptr;
        hr = pConsoleLnkDataList->CopyDataBlock(NT_CONSOLE_PROPS_SIG, (void**)&pNtConsoleProps);
        if (SUCCEEDED(hr))
        {
            wprintf(L"V1 Properties:\n");
            wprintf(L"\twFillAttribute: %x\n", pNtConsoleProps->wFillAttribute);
            wprintf(L"\twPopupFillAttribute: %x\n", pNtConsoleProps->wPopupFillAttribute);
            DumpCoord(L"dwScreenBufferSize", pNtConsoleProps->dwScreenBufferSize);
            DumpCoord(L"dwWindowSize", pNtConsoleProps->dwWindowSize);
            DumpCoord(L"dwWindowOrigin", pNtConsoleProps->dwWindowOrigin);
            wprintf(L"\tnFont: %x\n", pNtConsoleProps->nFont);
            wprintf(L"\tnInputBufferSize: %x\n", pNtConsoleProps->nInputBufferSize);
            DumpCoord(L"dwWindowOrigin", pNtConsoleProps->dwWindowOrigin);
            DumpCoord(L"dwFontSize", pNtConsoleProps->dwFontSize);
            wprintf(L"\tuFontFamily: %d\n", pNtConsoleProps->uFontFamily);
            wprintf(L"\tuFontWeight: %d\n", pNtConsoleProps->uFontWeight);
            wprintf(L"\tFaceName: \"%s\"\n", pNtConsoleProps->FaceName);
            wprintf(L"\tuCursorSize: %d\n", pNtConsoleProps->uCursorSize);
            DumpBool(L"bFullScreen", pNtConsoleProps->bFullScreen);
            DumpBool(L"bQuickEdit", pNtConsoleProps->bQuickEdit);
            DumpBool(L"bInsertMode", pNtConsoleProps->bInsertMode);
            DumpBool(L"bAutoPosition", pNtConsoleProps->bAutoPosition);
            wprintf(L"\tuHistoryBufferSize: %d\n", pNtConsoleProps->uHistoryBufferSize);
            wprintf(L"\tuNumberOfHistoryBuffers: %d\n", pNtConsoleProps->uNumberOfHistoryBuffers);
            DumpBool(L"bHistoryNoDup", pNtConsoleProps->bHistoryNoDup);
            wprintf(L"\tColorTable:\n");
            for (int iCurrColor = 0; iCurrColor < 16; iCurrColor++)
            {
                wprintf(L"\t\t%d:\t(R:%d\tG:\t%d\tB:\t%d)\n",
                        iCurrColor,
                        GetRValue(pNtConsoleProps->ColorTable[iCurrColor]),
                        GetGValue(pNtConsoleProps->ColorTable[iCurrColor]),
                        GetBValue(pNtConsoleProps->ColorTable[iCurrColor]));
            }
            LocalFree(pNtConsoleProps);
        }

        if (SUCCEEDED(hr))
        {
            // now dump East Asian properties if we can
            NT_FE_CONSOLE_PROPS* pNtFEConsoleProps;
            if (SUCCEEDED(pConsoleLnkDataList->CopyDataBlock(NT_FE_CONSOLE_PROPS_SIG,
                                                             (void**)&pNtFEConsoleProps)))
            {
                wprintf(L"\tuCodePage: %d", pNtFEConsoleProps->uCodePage);
                LocalFree(pNtFEConsoleProps);
            }
            else
            {
                wprintf(L"\t.lnk doesn't contain an explicit codepage setting.\n");
            }
        }

        pConsoleLnkDataList->Release();
    }

    return hr;
}

HRESULT DumpProperties(_In_ PCWSTR pszLnkFile)
{
    IShellLink* pslConsole;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&pslConsole));
    if (SUCCEEDED(hr))
    {
        IPersistFile* pPersistFileLnk;
        hr = pslConsole->QueryInterface(IID_PPV_ARGS(&pPersistFileLnk));
        if (SUCCEEDED(hr))
        {
            hr = pPersistFileLnk->Load(pszLnkFile, 0 /*grfMode*/);
            if (SUCCEEDED(hr))
            {
                hr = DumpV1Properties(pslConsole);
                if (SUCCEEDED(hr))
                {
                    wprintf(L"\n");
                    DumpV2Properties(pslConsole);
                }
                else if (hr == E_FAIL)
                {
                    wprintf(L"ERROR: .lnk file does not contain console properties.\n");
                }
            }
            else
            {
                wprintf(L"ERROR: Failed to load from lnk file (HRESULT: 0x%08x)\n", hr);
            }

            pPersistFileLnk->Release();
        }

        pslConsole->Release();
    }

    return hr;
}

int __cdecl wmain(int argc, WCHAR* argv[])
{
    if (argc == 2)
    {
        PCWSTR pszLnkFile = argv[1];
        wprintf(L"%s: Dumping lnk details for \"%s\"\n\n", PathFindFileName(argv[0]), pszLnkFile);
        if (PathFileExists(pszLnkFile))
        {
            HRESULT hr = CoInitialize(NULL);
            if (SUCCEEDED(hr))
            {
                hr = DumpProperties(pszLnkFile);
                CoUninitialize();
            }
        }
        else
        {
            wprintf(L"ERROR: Unable to open file: \"%s\". File does not exist.\n", pszLnkFile);
            return 1;
        }
    }
    else
    {
        PrintUsage();
    }
    return 0;
}
