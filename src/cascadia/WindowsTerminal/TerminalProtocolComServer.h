// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wrl/implements.h>
#include <wrl/client.h>

#include "ITerminalProtocol.h"

// Per-brand CLSIDs — same pattern as CTerminalHandoff.
#if defined(WT_BRANDING_RELEASE)
#define __CLSID_TerminalProtocolServer "832FDEC7-AA6F-4BAB-85FA-A491405638FC"
#elif defined(WT_BRANDING_PREVIEW)
#define __CLSID_TerminalProtocolServer "D77C8A1A-83C0-42FC-BADF-9BE82E2A1624"
#elif defined(WT_BRANDING_CANARY)
#define __CLSID_TerminalProtocolServer "264DE65B-F597-4183-8A76-6A039A604725"
#else
#define __CLSID_TerminalProtocolServer "AD9425AA-1722-4E7B-A451-AA1D09106E83"
#endif

class WindowEmperor;

// Classic-COM server for ITerminalProtocol. Marshaled by the OpenConsoleProxy
// proxy/stub (NOT WinRT Metadata-Based Marshaling), so activation/marshaling
// never goes through the combase WinRT activation catalog
// (CWinRTActivationStoreCatalog) implicated in the 0xc0000005 / 0x80010105
// failures. Complex results cross the wire as JSON (BSTR); the per-method logic
// and the UI-thread-marshaled page queries are unchanged from the WinRT server.
struct __declspec(uuid(__CLSID_TerminalProtocolServer))
TerminalProtocolComServer : public Microsoft::WRL::RuntimeClass<
                                Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
                                ITerminalProtocol>
{
    // ── ITerminalProtocol ──
    STDMETHODIMP GetCapabilities(BSTR* json) override;
    STDMETHODIMP GetActivePane(BSTR* json) override;
    STDMETHODIMP ListWindows(BSTR* json) override;
    STDMETHODIMP ListTabs(unsigned __int64 windowIdFilter, BSTR* json) override;
    STDMETHODIMP ListPanes(unsigned __int64 windowIdFilter, unsigned long tabIdFilter, BSTR* json) override;
    STDMETHODIMP ReadPaneOutput(GUID sessionId, BSTR source, long maxLines, BSTR* json) override;
    STDMETHODIMP GetProcessStatus(GUID sessionId, BSTR* json) override;
    STDMETHODIMP GetSessionVariable(GUID sessionId, BSTR name, BSTR* json) override;
    STDMETHODIMP GetSettings(BSTR* json) override;
    STDMETHODIMP CreateTab(unsigned __int64 windowId, BSTR profile, BSTR commandline, BSTR title, BSTR startingDirectory, boolean suppressAppTitle, boolean background, BSTR* json) override;
    STDMETHODIMP SplitPane(GUID sessionId, BSTR direction, float size, BSTR profile, BSTR commandline, boolean background, BSTR* json) override;
    STDMETHODIMP ClosePane(GUID sessionId) override;
    STDMETHODIMP SendInput(GUID sessionId, BSTR text) override;
    STDMETHODIMP FocusPane(GUID sessionId) override;
    STDMETHODIMP SetSessionVariable(GUID sessionId, BSTR name, BSTR value) override;

    // Static setup — must be called before s_StartListening().
    static void s_setEmperor(WindowEmperor* emperor) noexcept;

    static HRESULT s_StartListening();
    static HRESULT s_StopListening();

private:

    static WindowEmperor* s_emperor;
};
