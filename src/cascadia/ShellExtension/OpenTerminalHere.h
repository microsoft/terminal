/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OpenTerminalHere.h

Abstract:
- This is the class that implements our Explorer Context Menu item. By
  implementing IExplorerCommand, we can provide an entry to the context menu to
  allow the user to open the Terminal in the current working directory.
- Importantly, we need to make sure to declare the GUID of this implementation
  class explicitly, so we can refer to it in our manifest, and use it to create
  instances of this class when the shell asks for one.
- This is defined as a WRL type, so that we can use WRL's CoCreatableClass magic
  to create the class factory and module management for us. See more details in
  dllmain.cpp.

Author(s):
- Mike Griese - May 2020

--*/
#pragma once

using namespace Microsoft::WRL;

struct
#if defined(WT_BRANDING_RELEASE)
    __declspec(uuid("9f156763-7844-4dc4-b2b1-901f640f5155"))
#elif defined(WT_BRANDING_PREVIEW)
    __declspec(uuid("02db545a-3e20-46de-83a5-1329b1e88b6b"))
#else // DEV
    __declspec(uuid("52065414-e077-47ec-a3ac-1cc5455e1b54"))
#endif
        OpenTerminalHere : public RuntimeClass<RuntimeClassFlags<ClassicCom | InhibitFtmBase>, IExplorerCommand, IObjectWithSite>
{
#pragma region IExplorerCommand
    STDMETHODIMP Invoke(IShellItemArray* psiItemArray,
                        IBindCtx* pBindContext);
    STDMETHODIMP GetToolTip(IShellItemArray* psiItemArray,
                            LPWSTR* ppszInfoTip);
    STDMETHODIMP GetTitle(IShellItemArray* psiItemArray,
                          LPWSTR* ppszName);
    STDMETHODIMP GetState(IShellItemArray* psiItemArray,
                          BOOL fOkToBeSlow,
                          EXPCMDSTATE* pCmdState);
    STDMETHODIMP GetIcon(IShellItemArray* psiItemArray,
                         LPWSTR* ppszIcon);
    STDMETHODIMP GetFlags(EXPCMDFLAGS* pFlags);
    STDMETHODIMP GetCanonicalName(GUID* pguidCommandName);
    STDMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum);
#pragma endregion
#pragma region IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* site) noexcept;
    IFACEMETHODIMP GetSite(REFIID riid, void** site) noexcept;
#pragma endregion

private:
    HRESULT GetLocationFromSite(IShellItem** location) const noexcept;
    HRESULT GetBestLocationFromSelectionOrSite(IShellItemArray* psiArray, IShellItem** location) const noexcept;
    bool IsControlAndShiftPressed();

    wil::com_ptr_nothrow<IUnknown> site_;
};

CoCreatableClass(OpenTerminalHere);
