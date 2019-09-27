// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <wrl\implements.h>
#include <wrl\module.h>
#include <wil\resource.h>

#include <shlwapi.h>
#include <shellapi.h>

#include <shlguid.h>

#define PEMAGIC ((WORD)'P' + ((WORD)'E' << 8))
static CONSOLE_STATE_INFO g_csi;

using namespace Microsoft::WRL;

// This class exposes console property sheets for use when launching the filesystem shortcut properties dialog.
// clang-format off
[uuid(D2942F8E-478E-41D3-870A-35A16238F4EE)]
class ConsolePropertySheetHandler WrlFinal : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                                                                 IShellExtInit,
                                                                 IShellPropSheetExt,
                                                                 IPersist,
                                                                 FtmBase>
// clang-format on
{
public:
    HRESULT RuntimeClassInitialize()
    {
        return S_OK;
    }

    // IPersist
    STDMETHODIMP GetClassID(_Out_ CLSID* clsid) override
    {
        *clsid = __uuidof(this);
        return S_OK;
    }

    // IShellExtInit
    // Shell QI's for IShellExtInit and calls Initialize first. If we return a succeeding HRESULT, the shell will QI for
    // IShellPropSheetExt and call AddPages. A failing HRESULT causes the shell to skip us.
    STDMETHODIMP Initialize(_In_ PCIDLIST_ABSOLUTE /*pidlFolder*/, _In_ IDataObject* pdtobj, _In_ HKEY /*hkeyProgID*/)
    {
        WCHAR szLinkFileName[MAX_PATH];
        HRESULT hr = _ShouldAddPropertySheet(pdtobj, szLinkFileName, ARRAYSIZE(szLinkFileName));
        if (SUCCEEDED(hr))
        {
            hr = InitializeConsoleState() ? S_OK : E_FAIL;
            if (SUCCEEDED(hr))
            {
                hr = _InitializeGlobalStateInfo(szLinkFileName);
            }
        }

        return hr;
    }

    // IShellPropSheetExt
    STDMETHODIMP AddPages(_In_ LPFNADDPROPSHEETPAGE pfnAddPage, _In_ LPARAM lParam)
    {
        PROPSHEETPAGE psp[NUMBER_OF_PAGES] = {};
        HRESULT hr = PopulatePropSheetPageArray(psp, ARRAYSIZE(psp), TRUE /*fRegisterCallbacks*/) ? S_OK : E_FAIL;
        if (SUCCEEDED(hr))
        {
            for (UINT ipsp = 0; ipsp < ARRAYSIZE(psp) && SUCCEEDED(hr); ipsp++)
            {
                HPROPSHEETPAGE hPage = CreatePropertySheetPage(&psp[ipsp]);
                hr = (hPage == nullptr) ? E_FAIL : S_OK;
                if (SUCCEEDED(hr))
                {
                    pfnAddPage(hPage, lParam);
                }
            }
        }

        return hr;
    }

    STDMETHODIMP ReplacePage(_In_ UINT /*uPageID*/, _In_ LPFNADDPROPSHEETPAGE /*pfnReplacePage*/, _In_ LPARAM /*lParam*/)
    {
        // Implementation not needed -- MSDN says "Replaces a page in a property sheet for a Control Panel object.",
        // which we don't need to do.
        return E_NOTIMPL;
    }

private:
    ~ConsolePropertySheetHandler() = default;

    HRESULT _InitializeGlobalStateInfo(_In_ PCWSTR pszLinkFileName)
    {
        g_fHostedInFileProperties = TRUE;
        gpStateInfo = &g_csi;

        // Initialize the fIsV2Console with whatever the current v2 seeting is
        // in the registry. Usually this is set by conhost, but in this path,
        // we're being launched straight from explorer. See GH#2319, GH#2651
        gpStateInfo->fIsV2Console = GetConsoleBoolValue(CONSOLE_REGISTRY_FORCEV2, TRUE);

        InitRegistryValues(gpStateInfo);
        gpStateInfo->Defaults = TRUE;
        GetRegistryValues(gpStateInfo);

        PWSTR pszAllocatedFileName;
        HRESULT hr = SHStrDup(pszLinkFileName, &pszAllocatedFileName);
        if (SUCCEEDED(hr))
        {
            hr = StringCchCopyW(pszAllocatedFileName, MAX_PATH, pszLinkFileName);
            if (SUCCEEDED(hr))
            {
                // gpStateInfo now owns lifetime of the allocated filename
                gpStateInfo->LinkTitle = pszAllocatedFileName;
                pszAllocatedFileName = nullptr;

                // Not all console shortcuts have console-specific properties. We just take the registry defaults in
                // those cases.
                BOOL readSettings = FALSE;
                NTSTATUS s = ShortcutSerialization::s_GetLinkValues(gpStateInfo, &readSettings, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr);
                hr = HRESULT_FROM_NT(s);
            }
            else
            {
                CoTaskMemFree(pszAllocatedFileName);
            }
        }

        if (SUCCEEDED(hr))
        {
            InitializeFonts();
            hr = FindFontAndUpdateState();
        }

        return hr;
    }

    ///////////////////////////////////////////////////////////////////////////
    // CODE FROM THE SHELL DEPOT'S `idllib.h`
    // get a link target item without resolving it.
    HRESULT GetTargetIdList(_In_ IShellItem* psiLink, _COM_Outptr_ PIDLIST_ABSOLUTE* ppidl)
    {
        *ppidl = nullptr;

        IShellLink* psl;
        HRESULT hr = psiLink->BindToHandler(NULL, BHID_SFUIObject, IID_PPV_ARGS(&psl));
        if (SUCCEEDED(hr))
        {
            hr = psl->GetIDList(ppidl);
            if (SUCCEEDED(hr) && (*ppidl == nullptr))
            {
                hr = E_FAIL;
            }
            psl->Release();
        }
        return hr;
    }
    HRESULT GetTargetItem(_In_ IShellItem* psiLink, _In_ REFIID riid, _COM_Outptr_ void** ppv)
    {
        *ppv = nullptr;

        PIDLIST_ABSOLUTE pidl;
        HRESULT hr = GetTargetIdList(psiLink, &pidl);
        if (SUCCEEDED(hr))
        {
            hr = SHCreateItemFromIDList(pidl, riid, ppv);
            ILFree(pidl);
        }
        return hr;
    }
    ///////////////////////////////////////////////////////////////////////////

    HRESULT _GetShellItemLinkTargetExpanded(_In_ IShellItem* pShellItem,
                                            _Out_writes_(cchFilePathExtended) PWSTR pszFilePathExtended,
                                            const size_t cchFilePathExtended)
    {
        ComPtr<IShellItem> shellItemLinkTarget;
        HRESULT hr = GetTargetItem(pShellItem, IID_PPV_ARGS(&shellItemLinkTarget));
        if (SUCCEEDED(hr))
        {
            wil::unique_cotaskmem_string linkTargetPath;
            hr = shellItemLinkTarget->GetDisplayName(SIGDN_FILESYSPATH, &linkTargetPath);
            if (SUCCEEDED(hr))
            {
                hr = StringCchCopy(pszFilePathExtended, cchFilePathExtended, linkTargetPath.get());
            }
        }

        return hr;
    }

    HRESULT _ShouldAddPropertySheet(_In_ IDataObject* pdtobj,
                                    _Out_writes_(cchLinkFileName) PWSTR pszLinkFileName,
                                    const size_t cchLinkFileName)
    {
        ComPtr<IShellItemArray> shellItemArray;
        HRESULT hr = SHCreateShellItemArrayFromDataObject(pdtobj, IID_PPV_ARGS(&shellItemArray));
        if (SUCCEEDED(hr))
        {
            DWORD dwItemCount;
            hr = shellItemArray->GetCount(&dwItemCount);
            if (SUCCEEDED(hr))
            {
                // only consider being available for selections of a single file
                hr = dwItemCount == 1 ? S_OK : E_FAIL;
                if (SUCCEEDED(hr))
                {
                    ComPtr<IShellItem> shellItem;
                    hr = shellItemArray->GetItemAt(0, &shellItem);
                    if (SUCCEEDED(hr))
                    {
                        // First expensive portion of this method -- reads .lnk file
                        WCHAR szFileExpanded[MAX_PATH];
                        hr = _GetShellItemLinkTargetExpanded(shellItem.Get(), szFileExpanded, ARRAYSIZE(szFileExpanded));
                        if (SUCCEEDED(hr))
                        {
                            // Second expensive portion of this method -- cracks the PE header of the .lnk file target
                            // if it's an executable
                            SHFILEINFO sfi = { 0 };
                            DWORD_PTR dwFileType = SHGetFileInfo(szFileExpanded,
                                                                 0 /*dwFileAttributes*/,
                                                                 &sfi,
                                                                 sizeof(sfi),
                                                                 SHGFI_EXETYPE);
                            if (HIWORD(dwFileType) == 0 &&
                                LOWORD(dwFileType) == PEMAGIC)
                            {
                                // link target is a console application -- we should show our UI
                                hr = S_OK;
                            }
                            else
                            {
                                // link target is not a console application -- we should not show our UI
                                hr = E_FAIL;
                            }
                        }
                    }

                    if (hr == S_OK)
                    {
                        // We're going to show the UI, write out the link filename while we're here. This is needed
                        // so we know where changes should be written.
                        wil::unique_cotaskmem_string linkDisplayName;
                        hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &linkDisplayName);
                        if (SUCCEEDED(hr))
                        {
                            hr = StringCchCopy(pszLinkFileName, cchLinkFileName, linkDisplayName.get());
                        }
                    }
                }
            }
        }

        return hr;
    }
};
CoCreatableClass(ConsolePropertySheetHandler);
