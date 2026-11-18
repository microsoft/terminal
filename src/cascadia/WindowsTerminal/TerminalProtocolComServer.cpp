// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "TerminalProtocolComServer.h"
#include "WindowEmperor.h"
#include "AppHost.h"

#include <WtExeUtils.h>
#include "../../types/inc/utils.hpp"

#include <json/json.h>
#include "../TerminalProtocol/ProtocolParsing.h"
#include "WindowTarget.h"

#include <algorithm>
#include <thread>
#include <vector>

#include <wrl/module.h>
#include <wil/resource.h>
#include <wil/token_helpers.h>

using namespace Microsoft::WRL;

namespace ProtocolParsing = Microsoft::Terminal::Protocol::Parsing;

namespace Protocol = winrt::Microsoft::Terminal::Protocol;

WindowEmperor* TerminalProtocolComServer::s_emperor = nullptr;

// Returns the CLSID to register/activate the protocol server.
// Packaged: the compile-time per-brand CLSID (unique by package identity).
// Unpackaged/portable: a per-install-path v5 UUID so that multiple portable
// installations in different directories don't share the same registration.
static const GUID& _protocolClsid() noexcept
{
    if (IsPackaged())
        return __uuidof(TerminalProtocolComServer);

    static const GUID portableClsid = []() {
        std::filesystem::path dir{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        dir.remove_filename();
        return ::Microsoft::Console::Utils::CreateV5Uuid(__uuidof(TerminalProtocolComServer), std::as_bytes(std::span{ dir.native() }));
    }();
    return portableClsid;
}

static DWORD g_comRegistration = 0;
static std::shared_mutex g_mtx;
static std::thread g_comMtaThread;
static wil::unique_event g_comMtaStop;

void TerminalProtocolComServer::s_setEmperor(WindowEmperor* emperor) noexcept
{
    s_emperor = emperor;
}

HRESULT TerminalProtocolComServer::s_StartListening()
try
{
    std::unique_lock lock{ g_mtx };

    // The class factory must be registered on an MTA thread, not the STA/UI
    // thread. Every ITerminalProtocol method is a synchronous COM call that
    // internally creates an IAsyncOperation, calls .get() to block until it
    // completes, then returns the result. Each coroutine needs to resume on the
    // UI thread via co_await resume_foreground. If registration were on the
    // STA/UI thread, .get() would deadlock: the UI thread would be blocked
    // waiting for the coroutine, while the coroutine waits for the UI thread.
    g_comMtaStop.create(wil::EventOptions::ManualReset);

    wil::unique_event ready(wil::EventOptions::ManualReset);
    HRESULT regHr = S_OK;

    g_comMtaThread = std::thread([&ready, &regHr]() {
        auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

        // Classic-COM class factory marshaled via the OpenConsoleProxy proxy/stub
        const auto factory = Make<SimpleClassFactory<TerminalProtocolComServer>>();
        if (!factory)
        {
            regHr = E_OUTOFMEMORY;
        }
        else
        {
            ComPtr<IUnknown> unk;
            regHr = factory.As(&unk);
            if (SUCCEEDED(regHr))
            {
                regHr = CoRegisterClassObject(
                    _protocolClsid(),
                    unk.Get(),
                    CLSCTX_LOCAL_SERVER,
                    REGCLS_MULTIPLEUSE,
                    &g_comRegistration);
            }
        }

        ready.SetEvent();

        // Keep this MTA thread alive so the COM registration stays active.
        WaitForSingleObject(g_comMtaStop.get(), INFINITE);
    });

    ready.wait();
    RETURN_IF_FAILED(regHr);
    return S_OK;
}
CATCH_RETURN()

HRESULT TerminalProtocolComServer::s_StopListening()
{
    std::unique_lock lock{ g_mtx };

    if (g_comRegistration)
    {
        RETURN_IF_FAILED(CoRevokeClassObject(g_comRegistration));
        g_comRegistration = 0;
    }

    // Signal the MTA thread to exit
    if (g_comMtaStop)
    {
        g_comMtaStop.SetEvent();
    }
    if (g_comMtaThread.joinable())
    {
        g_comMtaThread.join();
    }

    return S_OK;
}

// ============================================================================
// Helpers
// ============================================================================

static DWORD _getIntegrityRid(HANDLE token)
{
    const auto label = wil::get_token_information<TOKEN_MANDATORY_LABEL>(token);
    const auto sid = label->Label.Sid;
    THROW_HR_IF(E_ACCESSDENIED, !IsValidSid(sid));

    const auto subAuthorityCount = *GetSidSubAuthorityCount(sid);
    THROW_HR_IF(E_ACCESSDENIED, subAuthorityCount == 0);
    return *GetSidSubAuthority(sid, subAuthorityCount - 1);
}

static HRESULT _authorizeCaller() noexcept
{
    const auto impersonateHr = CoImpersonateClient();
    if (FAILED(impersonateHr))
    {
        LOG_HR(impersonateHr);
        return E_ACCESSDENIED;
    }

    bool needsRevert = true;
    const auto revertOnExit = wil::scope_exit([&]() {
        if (needsRevert)
        {
            LOG_IF_FAILED(CoRevertToSelf());
        }
    });

    bool authorized = false;
    try
    {
        wil::unique_handle callerToken;
        THROW_IF_WIN32_BOOL_FALSE(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, callerToken.put()));

        wil::unique_handle serverToken{ GetCurrentProcessToken() };
        const auto callerUser = wil::get_token_information<TOKEN_USER>(callerToken.get());
        const auto serverUser = wil::get_token_information<TOKEN_USER>(serverToken.get());

        THROW_HR_IF(E_ACCESSDENIED, !IsValidSid(callerUser->User.Sid));
        THROW_HR_IF(E_ACCESSDENIED, !IsValidSid(serverUser->User.Sid));

        authorized = EqualSid(callerUser->User.Sid, serverUser->User.Sid) &&
                     _getIntegrityRid(callerToken.get()) >= _getIntegrityRid(serverToken.get());
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    const auto revertHr = CoRevertToSelf();
    if (FAILED(revertHr))
    {
        LOG_HR(revertHr);
        return E_ACCESSDENIED;
    }
    needsRevert = false;

    return authorized ? S_OK : E_ACCESSDENIED;
}

static winrt::TerminalApp::TerminalPage _getPage(AppHost* host)
{
    if (!host)
        return nullptr;
    const auto logic = host->Logic();
    if (!logic)
        return nullptr;
    const auto root = logic.GetRoot();
    if (!root)
        return nullptr;
    return root.try_as<winrt::TerminalApp::TerminalPage>();
}

static bool _parseJson(const std::string& str, Json::Value& out)
{
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream ss(str);
    return Json::parseFromStream(rb, ss, &out, &errs);
}

// JSON serialization helpers. The wire format is JSON; see wtcli Formatting.

static std::string _guidStr(const winrt::guid& g)
{
    wchar_t buf[40]{};
    ::StringFromGUID2(g, buf, ARRAYSIZE(buf));
    std::wstring ws{ buf };
    if (ws.size() > 2 && ws.front() == L'{' && ws.back() == L'}')
        ws = ws.substr(1, ws.size() - 2);
    return winrt::to_string(winrt::hstring{ ws });
}

// Serialize a Json::Value to a compact BSTR.
static BSTR _bstrFromJson(const Json::Value& v)
{
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    BSTR b = ::SysAllocString(winrt::to_hstring(Json::writeString(wb, v)).c_str());
    THROW_IF_NULL_ALLOC(b);
    return b;
}

static Json::Value _toJson(const Protocol::WindowInfo& w)
{
    Json::Value v;
    v["window_id"] = static_cast<Json::UInt64>(w.WindowId);
    v["name"] = winrt::to_string(w.Name);
    v["title"] = winrt::to_string(w.Title);
    v["is_focused"] = static_cast<bool>(w.IsFocused);
    v["tab_count"] = static_cast<Json::UInt>(w.TabCount);
    return v;
}

static Json::Value _toJson(const Protocol::TabInfo& t)
{
    Json::Value v;
    v["tab_id"] = static_cast<Json::UInt>(t.TabId);
    v["window_id"] = static_cast<Json::UInt64>(t.WindowId);
    v["title"] = winrt::to_string(t.Title);
    v["is_active"] = static_cast<bool>(t.IsActive);
    v["pane_count"] = static_cast<Json::UInt>(t.PaneCount);
    return v;
}

static Json::Value _toJson(const Protocol::PaneInfo& p)
{
    Json::Value v;
    v["session_id"] = _guidStr(p.SessionId);
    v["tab_id"] = static_cast<Json::UInt>(p.TabId);
    v["window_id"] = static_cast<Json::UInt64>(p.WindowId);
    v["title"] = winrt::to_string(p.Title);
    v["profile"] = winrt::to_string(p.Profile);
    v["is_active"] = static_cast<bool>(p.IsActive);
    v["is_agent_pane"] = static_cast<bool>(p.IsAgentPane);
    v["pid"] = static_cast<Json::UInt>(p.Pid);
    v["size"]["rows"] = p.Rows;
    v["size"]["columns"] = p.Columns;
    v["cwd"] = winrt::to_string(p.Cwd);
    return v;
}

static Json::Value _toJson(const Protocol::ProcessStatus& s)
{
    Json::Value v;
    v["session_id"] = _guidStr(s.SessionId);
    v["state"] = winrt::to_string(s.State);
    v["pid"] = static_cast<Json::UInt>(s.Pid);
    v["has_exit_code"] = static_cast<bool>(s.HasExitCode);
    if (s.HasExitCode)
        v["exit_code"] = s.ExitCode;
    return v;
}

static Json::Value _toJson(const Protocol::TabCreationResult& r)
{
    Json::Value v;
    v["tab_id"] = static_cast<Json::UInt>(r.TabId);
    v["session_id"] = _guidStr(r.SessionId);
    v["window_id"] = static_cast<Json::UInt64>(r.WindowId);
    v["pid"] = static_cast<Json::UInt>(r.Pid);
    v["pending"] = static_cast<bool>(r.Pending);
    return v;
}

// Convert an [in] BSTR to a winrt::hstring (null-safe).
static winrt::hstring _hstr(BSTR b)
{
    return b ? winrt::hstring{ b } : winrt::hstring{};
}

// Resolve a window target for a read-only query. "new" (and negative IDs) are
// rejected because a query should never have the side effect of creating a window.
static HRESULT _resolveWindowForQuery(WindowEmperor* emperor, BSTR windowTarget, const AppHost*& resolved) noexcept
try
{
    const auto target = ParseWindowTarget(_hstr(windowTarget));
    RETURN_HR_IF(E_INVALIDARG, target.Kind == WindowTargetKind::New);

    resolved = emperor->ResolveWindowTarget(target);
    RETURN_HR_IF(E_INVALIDARG, !resolved);
    return S_OK;
}
CATCH_RETURN()

// ============================================================================
// Queries
// ============================================================================

STDMETHODIMP TerminalProtocolComServer::GetActivePane(BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const auto host = s_emperor->GetMostRecentWindow();
    RETURN_HR_IF(E_FAIL, !host);

    const auto page = _getPage(host);
    RETURN_HR_IF(E_FAIL, !page);

    auto info = page.GetProtocolActivePane().get();
    RETURN_HR_IF(E_FAIL, info.SessionId == winrt::guid{});

    // TerminalPage doesn't know the window ID, so fill it in here.
    const auto& props = host->Logic().WindowProperties();
    info.WindowId = props.WindowId();

    *json = _bstrFromJson(_toJson(info));
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::ListWindows(BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const auto mostRecent = s_emperor->GetMostRecentWindow();
    Json::Value arr(Json::arrayValue);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto logic = host->Logic();
        if (!logic)
            continue;

        const auto& props = logic.WindowProperties();

        Protocol::WindowInfo info{};
        info.WindowId = props.WindowId();
        info.Name = props.WindowName();
        info.IsFocused = (host.get() == mostRecent);

        const auto page = _getPage(host.get());
        info.TabCount = page ? page.TabCount() : 0;
        info.Title = page ? page.WindowTitle() : winrt::hstring{};
        arr.append(_toJson(info));
    }

    *json = _bstrFromJson(arr);
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::ListTabs(BSTR windowTarget, boolean allWindows, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const AppHost* filter = nullptr;
    if (!allWindows)
    {
        RETURN_IF_FAILED(_resolveWindowForQuery(s_emperor, windowTarget, filter));
    }

    Json::Value arr(Json::arrayValue);

    for (const auto& host : s_emperor->GetWindows())
    {
        if (filter && host.get() != filter)
            continue;

        const auto logic = host->Logic();
        if (!logic)
            continue;

        const auto page = _getPage(host.get());
        if (!page)
            continue;

        const auto windowId = logic.WindowProperties().WindowId();
        const auto tabs = page.GetProtocolTabs().get();
        for (uint32_t i = 0; i < tabs.Size(); ++i)
        {
            auto t = tabs.GetAt(i);
            t.WindowId = windowId;
            arr.append(_toJson(t));
        }
    }

    *json = _bstrFromJson(arr);
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::ListPanes(BSTR windowTarget, boolean allWindows, unsigned long tabIdFilter, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const AppHost* filter = nullptr;
    if (!allWindows)
    {
        RETURN_IF_FAILED(_resolveWindowForQuery(s_emperor, windowTarget, filter));
    }

    Json::Value arr(Json::arrayValue);

    for (const auto& host : s_emperor->GetWindows())
    {
        if (filter && host.get() != filter)
            continue;

        const auto logic = host->Logic();
        if (!logic)
            continue;

        const auto page = _getPage(host.get());
        if (!page)
            continue;

        const auto windowId = logic.WindowProperties().WindowId();
        const auto panes = page.GetProtocolPanes(tabIdFilter).get();
        for (uint32_t i = 0; i < panes.Size(); ++i)
        {
            auto p = panes.GetAt(i);
            p.WindowId = windowId;
            arr.append(_toJson(p));
        }
    }

    *json = _bstrFromJson(arr);
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::GetProcessStatus(GUID sessionId, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        auto info = page.GetProtocolProcessStatus(winrt::guid{ sessionId }).get();
        if (info.SessionId != winrt::guid{})
        {
            *json = _bstrFromJson(_toJson(info));
            return S_OK;
        }
    }

    return E_FAIL;
}
CATCH_RETURN()

// ============================================================================
// Mutations
// ============================================================================

// Create a window for `target` and report its first tab.
//
// The window is created on the main thread via SendMessage, but its first tab
// is only built later during XAML's first layout pass. So we wait up to
// waitTimeoutMs for the page to report itself initialized and then read the
// tab back. If the caller didn't want to wait or the wait expired, we still
// succeeded in creating the window, so report it as Pending rather than failing,
// and let the caller follow up by WindowId.
static HRESULT _createTabInNewWindow(WindowEmperor* emperor,
                                     const WindowTarget& target,
                                     const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& args,
                                     unsigned long waitTimeoutMs,
                                     BSTR* json) noexcept
try
{
    WindowEmperor::ProtocolWindowRequest request{ &target, args, 0 };
    SendMessageW(emperor->GetMainWindowForProtocol(),
                 WindowEmperor::WM_CREATE_PROTOCOL_WINDOW,
                 0,
                 reinterpret_cast<LPARAM>(&request));
    RETURN_HR_IF(E_FAIL, request.CreatedWindowId == 0);

    Protocol::TabCreationResult result{};
    result.WindowId = request.CreatedWindowId;
    result.Pending = true;

    const auto reportPending = [&]() {
        *json = _bstrFromJson(_toJson(result));
        return S_OK;
    };

    if (waitTimeoutMs == 0)
    {
        return reportPending();
    }

    WindowTarget byId{ WindowTargetKind::Id, request.CreatedWindowId };
    const auto page = _getPage(emperor->ResolveWindowTarget(byId));
    if (!page || !page.WaitForInitialized(waitTimeoutMs).get())
    {
        return reportPending();
    }

    // Grab the first pane of the first tab
    const auto panes = page.GetProtocolPanes(0).get();
    if (!panes || panes.Size() == 0)
    {
        return reportPending();
    }

    const auto pane = panes.GetAt(0);
    result.TabId = pane.TabId;
    result.SessionId = pane.SessionId;
    result.Pid = pane.Pid;
    result.Pending = false;

    *json = _bstrFromJson(_toJson(result));
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::CreateTab(BSTR windowTarget,
                                                  BSTR profile,
                                                  BSTR commandline,
                                                  BSTR title,
                                                  BSTR startingDirectory,
                                                  boolean suppressAppTitle,
                                                  boolean background,
                                                  unsigned long waitTimeoutMs,
                                                  BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const auto target = ParseWindowTarget(_hstr(windowTarget));

    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs newTermArgs;
    const auto profileH = _hstr(profile);
    const auto commandlineH = _hstr(commandline);
    const auto titleH = _hstr(title);
    const auto startingDirectoryH = _hstr(startingDirectory);
    if (!profileH.empty())
        newTermArgs.Profile(profileH);
    if (!commandlineH.empty())
        newTermArgs.Commandline(commandlineH);
    if (!startingDirectoryH.empty())
        newTermArgs.StartingDirectory(startingDirectoryH);
    if (!titleH.empty())
    {
        newTermArgs.TabTitle(titleH);
        if (suppressAppTitle)
            newTermArgs.SuppressApplicationTitle(true);
    }

    auto* targetHost = s_emperor->ResolveWindowTarget(target);
    if (!targetHost)
    {
        RETURN_HR_IF(E_INVALIDARG, target.Kind == WindowTargetKind::Id);
        RETURN_HR_IF(E_FAIL, target.Kind == WindowTargetKind::Unspecified || target.Kind == WindowTargetKind::Existing);
        return _createTabInNewWindow(s_emperor, target, newTermArgs, waitTimeoutMs, json);
    }

    const auto page = _getPage(targetHost);
    RETURN_HR_IF(E_FAIL, !page);

    auto cr = page.CreateProtocolTab(newTermArgs, background != 0).get();
    RETURN_HR_IF(E_FAIL, cr.SessionId == winrt::guid{});

    const auto& props = targetHost->Logic().WindowProperties();
    cr.WindowId = props.WindowId();
    *json = _bstrFromJson(_toJson(cr));
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::SplitPane(GUID sessionId,
                                                  BSTR direction,
                                                  float size,
                                                  BSTR profile,
                                                  BSTR commandline,
                                                  boolean background,
                                                  BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    // Map direction string to SplitDirection enum via shared parsing logic.
    const auto parsedDir = ProtocolParsing::ParseSplitDirection(winrt::to_string(_hstr(direction)));
    auto splitDir = static_cast<winrt::Microsoft::Terminal::Settings::Model::SplitDirection>(
        static_cast<int>(parsedDir));

    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs newTermArgs;
    const auto profileH = _hstr(profile);
    const auto commandlineH = _hstr(commandline);
    if (!profileH.empty())
        newTermArgs.Profile(profileH);
    if (!commandlineH.empty())
        newTermArgs.Commandline(commandlineH);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        auto cr = page.SplitProtocolPane(winrt::guid{ sessionId }, splitDir, size, newTermArgs, background != 0).get();
        if (cr.SessionId == winrt::guid{})
            continue; // pane not in this window

        const auto& props = host->Logic().WindowProperties();
        cr.WindowId = props.WindowId();
        *json = _bstrFromJson(_toJson(cr));
        return S_OK;
    }

    return E_FAIL;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::ClosePane(GUID sessionId)
try
{
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        if (page.CloseProtocolPane(winrt::guid{ sessionId }).get())
            return S_OK;
    }

    return E_FAIL;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::FocusPane(GUID sessionId)
try
{
    RETURN_IF_FAILED(_authorizeCaller());
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        if (page.FocusProtocolPane(winrt::guid{ sessionId }).get())
            return S_OK;
    }

    return E_FAIL;
}
CATCH_RETURN()
