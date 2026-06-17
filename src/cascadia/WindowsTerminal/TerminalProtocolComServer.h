// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <mutex>
#include <vector>

#include <wrl/implements.h>
#include <wrl/client.h>

#include "ITerminalProtocol.h"

// Per-brand CLSIDs — same pattern as CTerminalHandoff. Reused unchanged from the
// previous WinRT/MBM server, so WT_COM_CLSID discovery on the client is identical.
#if defined(WT_BRANDING_RELEASE)
#define __CLSID_TerminalProtocolServer "A2E4F6B8-1C3D-4E5F-A6B7-C8D9E0F1A2B3"
#elif defined(WT_BRANDING_PREVIEW)
#define __CLSID_TerminalProtocolServer "B3F5A7C9-2D4E-4F6A-B7C8-D9E0F1A2B3C4"
#elif defined(WT_BRANDING_CANARY)
#define __CLSID_TerminalProtocolServer "C4A6B8D0-3E5F-4A7B-C8D9-E0F1A2B3C4D5"
#else
#define __CLSID_TerminalProtocolServer "D5B7C9E1-4F6A-4B8C-D9E0-F1A2B3C4D5E6"
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
    ~TerminalProtocolComServer();

    // ── ITerminalProtocol ──
    STDMETHODIMP Authenticate(BSTR token, BSTR* resultJson) override;
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
    STDMETHODIMP Subscribe(ITerminalProtocolEventSink* sink) override;
    STDMETHODIMP Unsubscribe() override;
    STDMETHODIMP SendEvent(BSTR eventJson) override;

    // Static setup — must be called before s_StartListening().
    static void s_setEmperor(WindowEmperor* emperor) noexcept;

    static HRESULT s_StartListening();
    static HRESULT s_StopListening();

    // Re-runs per-window page event registration after a new AppHost is added.
    static void s_OnWindowAdded(class AppHost* host);

    // Deliver an event to all connected COM clients.
    static void s_NotifyEventToComClients(const std::string& eventJson);

private:
    bool _authenticated = false;

    // Per-instance event sink, stored as an agile reference so it can be
    // resolved + called from any apartment (set via Subscribe, cleared via
    // Unsubscribe).
    std::mutex _callbackMutex;
    Microsoft::WRL::ComPtr<IAgileReference> _sinkRef;

    // Static tracking of live COM instances for event delivery.
    static std::mutex s_instancesMutex;
    static std::vector<TerminalProtocolComServer*> s_instances;

    bool _instanceRegistered{ false };

    void _addInstance();
    void _removeInstance();
    static void _ensurePageEventsRegistered();

    // Per-method UI-thread dispatch helpers (unchanged from the WinRT server;
    // they marshal SendEvent payloads onto each window's TerminalPage).
    static void _dispatchAutofixStateToPage(const winrt::hstring& eventJson);
    static void _dispatchAgentStatusToPage(const winrt::hstring& eventJson);
    static void _dispatchCloseAgentPaneToPage(const winrt::hstring& eventJson);
    static void _dispatchAgentStateChangedToPage(const winrt::hstring& eventJson);
    static void _dispatchResumeInNewAgentTabToPage(const winrt::hstring& eventJson);
    static void _dispatchAgentChipTargetToPage(const winrt::hstring& eventJson);
    static void _dispatchRestartAgentStackToPage(const winrt::hstring& eventJson);
    static void _dispatchRestartAgentPaneToPage(const winrt::hstring& eventJson);

    static WindowEmperor* s_emperor;
};
