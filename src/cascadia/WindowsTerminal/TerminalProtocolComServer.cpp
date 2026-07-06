// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "TerminalProtocolComServer.h"
#include "WindowEmperor.h"
#include "AppHost.h"

#include <json/json.h>
#include <til/io.h>
#include "../TerminalProtocol/ProtocolParsing.h"

#include <algorithm>
#include <thread>
#include <vector>

#include <wrl/module.h>
#include <wil/resource.h>

using namespace Microsoft::WRL;

namespace ProtocolParsing = Microsoft::Terminal::Protocol::Parsing;

namespace Protocol = winrt::Microsoft::Terminal::Protocol;

// Static state — set once before registration, never mutated.
WindowEmperor* TerminalProtocolComServer::s_emperor = nullptr;

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

    // Register the COM class factory on a dedicated MTA thread so that
    // incoming COM calls are dispatched to MTA worker threads rather than
    // the STA/UI thread. This keeps long-running calls off the UI thread —
    // and is what lets each method co_await onto the UI thread and .get()
    // without blocking the UI thread.
    g_comMtaStop.create(wil::EventOptions::ManualReset);

    wil::unique_event ready(wil::EventOptions::ManualReset);
    HRESULT regHr = S_OK;

    g_comMtaThread = std::thread([&ready, &regHr]() {
        auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

        // Classic-COM class factory (WRL) — marshaled via the OpenConsoleProxy
        // proxy/stub, not WinRT MBM.
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
                    __uuidof(TerminalProtocolComServer),
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

// Parse a JSON string into Json::Value
static bool _parseJson(const std::string& str, Json::Value& out)
{
    Json::CharReaderBuilder rb;
    std::string errs;
    std::istringstream ss(str);
    return Json::parseFromStream(rb, ss, &out, &errs);
}

// ── JSON serialization helpers (the wire format is JSON; see wtcli Formatting) ──

static std::string _guidStr(const winrt::guid& g)
{
    wchar_t buf[40]{};
    ::StringFromGUID2(g, buf, ARRAYSIZE(buf));
    std::wstring ws{ buf };
    if (ws.size() > 2 && ws.front() == L'{' && ws.back() == L'}')
        ws = ws.substr(1, ws.size() - 2);
    return winrt::to_string(winrt::hstring{ ws });
}

// Allocate a BSTR from a UTF-8 std::string. Throws E_OUTOFMEMORY on allocation
// failure so the (CATCH_RETURN-wrapped) caller returns a failure HRESULT rather
// than S_OK with a null out-param.
static BSTR _bstr(const std::string& utf8)
{
    BSTR b = ::SysAllocString(winrt::to_hstring(utf8).c_str());
    THROW_IF_NULL_ALLOC(b);
    return b;
}

// Serialize a Json::Value to a compact BSTR.
static BSTR _bstrFromJson(const Json::Value& v)
{
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    return _bstr(Json::writeString(wb, v));
}

static Json::Value _toJson(const Protocol::WindowInfo& w)
{
    Json::Value v;
    v["window_id"] = static_cast<Json::UInt64>(w.WindowId);
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

static Json::Value _toJson(const Protocol::PaneOutput& o)
{
    Json::Value v;
    v["session_id"] = _guidStr(o.SessionId);
    v["content"] = winrt::to_string(o.Content);
    v["line_count"] = o.LineCount;
    v["truncated"] = static_cast<bool>(o.Truncated);
    v["has_marks"] = static_cast<bool>(o.HasMarks);
    return v;
}

static Json::Value _toJson(const Protocol::ProcessStatus& s)
{
    Json::Value v;
    v["session_id"] = _guidStr(s.SessionId);
    v["state"] = winrt::to_string(s.State);
    v["pid"] = static_cast<Json::UInt>(s.Pid);
    v["has_exit_code"] = static_cast<bool>(s.HasExitCode);
    // Only emit exit_code when it's meaningful; clients gate on has_exit_code,
    // so omitting it for a still-running process avoids a misleading 0.
    if (s.HasExitCode)
        v["exit_code"] = s.ExitCode;
    return v;
}

static Json::Value _toJson(const Protocol::SessionVariable& s)
{
    Json::Value v;
    v["session_id"] = _guidStr(s.SessionId);
    v["name"] = winrt::to_string(s.Name);
    v["value"] = winrt::to_string(s.Value);
    v["exists"] = static_cast<bool>(s.Exists);
    return v;
}

static Json::Value _toJson(const Protocol::TabCreationResult& r)
{
    Json::Value v;
    v["tab_id"] = static_cast<Json::UInt>(r.TabId);
    v["session_id"] = _guidStr(r.SessionId);
    v["window_id"] = static_cast<Json::UInt64>(r.WindowId);
    v["pid"] = static_cast<Json::UInt>(r.Pid);
    return v;
}

// Convert an [in] BSTR to a winrt::hstring (null-safe).
static winrt::hstring _hstr(BSTR b)
{
    return b ? winrt::hstring{ b } : winrt::hstring{};
}

// ============================================================================
// ITerminalProtocol — Meta
// ============================================================================

STDMETHODIMP TerminalProtocolComServer::GetCapabilities(BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;

    static const std::vector<std::string> supportedMethods = {
        "authenticate",
        "get_capabilities",
        "get_active_pane",
        "list_windows",
        "list_tabs",
        "list_panes",
        "read_pane_output",
        "get_process_status",
        "get_session_variable",
        "get_settings",
        "create_tab",
        "split_pane",
        "close_pane",
        "send_input",
        "focus_pane",
        "set_session_variable",
        "subscribe",
        "unsubscribe",
        "send_event",
    };

    Json::Value methods(Json::arrayValue);
    for (const auto& m : supportedMethods)
        methods.append(m);

    *json = _bstrFromJson(methods);
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
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const auto host = s_emperor->GetMostRecentWindow();
    RETURN_HR_IF(E_FAIL, !host);

    const auto page = _getPage(host);
    RETURN_HR_IF(E_FAIL, !page);

    auto info = page.GetProtocolActivePane().get();
    RETURN_HR_IF(E_FAIL, info.SessionId == winrt::guid{});

    // TerminalPage doesn't know the window ID — fill it in here.
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
        info.Title = props.WindowNameForDisplay();
        info.IsFocused = (host.get() == mostRecent);

        const auto page = _getPage(host.get());
        info.TabCount = page ? page.TabCount() : 0;
        arr.append(_toJson(info));
    }

    *json = _bstrFromJson(arr);
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::ListTabs(unsigned __int64 windowIdFilter, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    Json::Value arr(Json::arrayValue);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto logic = host->Logic();
        if (!logic)
            continue;

        const auto& props = logic.WindowProperties();
        if (windowIdFilter != 0 && props.WindowId() != windowIdFilter)
            continue;

        const auto page = _getPage(host.get());
        if (!page)
            continue;

        const auto windowId = props.WindowId();
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

STDMETHODIMP TerminalProtocolComServer::ListPanes(unsigned __int64 windowIdFilter, unsigned long tabIdFilter, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    Json::Value arr(Json::arrayValue);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto logic = host->Logic();
        if (!logic)
            continue;

        const auto& props = logic.WindowProperties();
        if (windowIdFilter != 0 && props.WindowId() != windowIdFilter)
            continue;

        const auto page = _getPage(host.get());
        if (!page)
            continue;

        const auto windowId = props.WindowId();
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

STDMETHODIMP TerminalProtocolComServer::ReadPaneOutput(GUID sessionId, BSTR source, long maxLines, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    const auto src = _hstr(source);
    const auto effectiveSource = src.empty() ? winrt::hstring{ L"scrollback" } : src;

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        auto info = page.ReadProtocolPaneOutput(winrt::guid{ sessionId }, effectiveSource, maxLines).get();
        if (info.SessionId != winrt::guid{})
        {
            *json = _bstrFromJson(_toJson(info));
            return S_OK;
        }
    }

    return E_FAIL; // Pane not found
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::GetProcessStatus(GUID sessionId, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
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

STDMETHODIMP TerminalProtocolComServer::GetSessionVariable(GUID sessionId, BSTR name, BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        auto info = page.GetProtocolSessionVariable(winrt::guid{ sessionId }, _hstr(name)).get();
        if (info.SessionId != winrt::guid{})
        {
            *json = _bstrFromJson(_toJson(info));
            return S_OK;
        }
    }

    return E_FAIL;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::GetSettings(BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;

    const std::filesystem::path settingsPath{
        std::wstring_view{ winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings::SettingsPath() }
    };
    *json = _bstr(til::io::read_file_as_utf8_string_if_exists(settingsPath));
    return S_OK;
}
CATCH_RETURN()

// ============================================================================
// Mutations
// ============================================================================

STDMETHODIMP TerminalProtocolComServer::CreateTab(unsigned __int64 windowId,
                                                  BSTR profile,
                                                  BSTR commandline,
                                                  BSTR title,
                                                  BSTR startingDirectory,
                                                  boolean suppressAppTitle,
                                                  boolean background,
                                                  BSTR* json)
try
{
    RETURN_HR_IF_NULL(E_POINTER, json);
    *json = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    // Find target window.
    AppHost* targetHost = nullptr;
    if (windowId != 0)
    {
        targetHost = s_emperor->GetWindowById(windowId);
    }
    else
    {
        targetHost = s_emperor->GetMostRecentWindow();
    }
    RETURN_HR_IF(E_FAIL, !targetHost);

    const auto page = _getPage(targetHost);
    RETURN_HR_IF(E_FAIL, !page);

    // Build NewTerminalArgs.
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
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    // Map direction string to SplitDirection enum via shared parsing logic.
    const auto parsedDir = ProtocolParsing::ParseSplitDirection(winrt::to_string(_hstr(direction)));
    auto splitDir = static_cast<winrt::Microsoft::Terminal::Settings::Model::SplitDirection>(
        static_cast<int>(parsedDir));

    // Build NewTerminalArgs.
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

STDMETHODIMP TerminalProtocolComServer::SendInput(GUID sessionId, BSTR text)
try
{
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    const auto textH = _hstr(text);

    // Empty input is a no-op, matching ControlCore::SendInput semantics so
    // COM clients that send "" don't see surprising E_INVALIDARG failures.
    if (textH.empty())
    {
        return S_OK;
    }

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        if (page.SendProtocolInput(winrt::guid{ sessionId }, textH).get())
            return S_OK;
    }

    return E_FAIL;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::FocusPane(GUID sessionId)
try
{
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

STDMETHODIMP TerminalProtocolComServer::SetSessionVariable(GUID sessionId, BSTR name, BSTR value)
try
{
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);
    RETURN_HR_IF(E_INVALIDARG, winrt::guid{ sessionId } == winrt::guid{});

    const auto nameH = _hstr(name);
    RETURN_HR_IF(E_INVALIDARG, nameH.empty());

    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
            continue;

        if (page.SetProtocolSessionVariable(winrt::guid{ sessionId }, nameH, _hstr(value)).get())
            return S_OK;
    }

    return E_FAIL;
}
CATCH_RETURN()


