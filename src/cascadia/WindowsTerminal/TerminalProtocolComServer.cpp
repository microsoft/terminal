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

// Static instance tracking for event delivery to COM clients
std::mutex TerminalProtocolComServer::s_instancesMutex;
std::vector<TerminalProtocolComServer*> TerminalProtocolComServer::s_instances;

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

TerminalProtocolComServer::~TerminalProtocolComServer()
{
    _removeInstance();
}

void TerminalProtocolComServer::_addInstance()
{
    std::lock_guard lock{ s_instancesMutex };
    if (_instanceRegistered)
        return;
    _instanceRegistered = true;
    s_instances.push_back(this);
}

void TerminalProtocolComServer::_removeInstance()
{
    std::lock_guard lock{ s_instancesMutex };
    std::erase(s_instances, this);
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

void TerminalProtocolComServer::_ensurePageEventsRegistered()
{
    if (!s_emperor)
        return;

    // Per-TerminalPage registration tracking. Each window gets its
    // ProtocolVtSequenceReceived wired exactly once, so events from any
    // window — not just the lexically-first one — reach the COM fan-out.
    //
    // Key on `winrt::weak_ref<TerminalPage>` rather than `AppHost*`:
    // - A closed window's AppHost is destructed and the same memory
    //   address may later be reused by a freshly-created AppHost. With a
    //   raw-pointer key the new window would be misidentified as
    //   "already registered" and silently skipped (ABA bug).
    // - `weak_ref` tracks the WinRT object's actual identity; dead
    //   entries surface as `weak_ref::get()` returning null, and new
    //   pages are recognized as distinct even if they share an address
    //   with a defunct one.
    static std::mutex s_regMutex;
    static std::vector<winrt::weak_ref<winrt::TerminalApp::TerminalPage>> s_registered;

    std::lock_guard lock{ s_regMutex };

    // Prune dead entries (page destructed → weak_ref returns null).
    std::erase_if(s_registered, [](const auto& w) { return !w.get(); });

    // Register any TerminalPage we haven't seen before. If the page
    // isn't ready yet (early startup race), skip silently — the next
    // Subscribe() or s_OnWindowAdded() call will retry.
    for (const auto& host : s_emperor->GetWindows())
    {
        const auto page = _getPage(host.get());
        if (!page)
        {
            continue;
        }

        const bool alreadyRegistered = std::any_of(
            s_registered.begin(),
            s_registered.end(),
            [&page](const auto& w) {
                const auto p = w.get();
                return p && p == page;
            });
        if (alreadyRegistered)
        {
            continue;
        }

        page.ProtocolVtSequenceReceived(
            [](auto&&, const winrt::hstring& eventJson) {
                s_NotifyEventToComClients(winrt::to_string(eventJson));
            });
        s_registered.push_back(winrt::make_weak(page));
    }
}

void TerminalProtocolComServer::s_OnWindowAdded(AppHost* /*host*/)
{
    _ensurePageEventsRegistered();
}

void TerminalProtocolComServer::s_NotifyEventToComClients(const std::string& eventJson)
{
    wil::unique_bstr eventBstr{ ::SysAllocString(winrt::to_hstring(eventJson).c_str()) };
    if (!eventBstr)
    {
        // OOM allocating the event payload — skip this delivery rather than
        // calling OnEvent with a null BSTR, which would break the documented
        // JSON-string event contract.
        return;
    }

    // Snapshot the agile sink references under lock, then invoke outside the
    // lock to avoid deadlocks if a callback reenters the server (e.g. via
    // SendEvent).
    std::vector<ComPtr<IAgileReference>> sinks;
    {
        std::lock_guard lock{ s_instancesMutex };
        for (auto* instance : s_instances)
        {
            std::lock_guard cbLock{ instance->_callbackMutex };
            if (instance->_sinkRef)
                sinks.push_back(instance->_sinkRef);
        }
    }

    // TODO: event fan-out runs on the UI/STA thread (ProtocolVtSequenceReceived
    // is raised there), and each OnEvent below is a SYNCHRONOUS cross-process
    // call. A slow or blocked subscriber (e.g. wtcli's stdout pipe full because
    // wta isn't draining it) therefore stalls the terminal UI thread, and the
    // cost is serial across all subscribers. For high event volume this should
    // be moved off the UI thread — queue events and drain them on a dedicated
    // background MTA thread (mind ordering, back-pressure, and subscriber
    // add/remove thread-safety), and/or give OnEvent a timeout. Tracked as
    // follow-up; not addressed in the WinRT->classic-COM migration.
    for (auto& ref : sinks)
    {
        // Resolve the agile reference to a sink proxy valid on THIS thread,
        // then call it — this is the cross-apartment-safe callback path.
        ComPtr<ITerminalProtocolEventSink> sink;
        if (FAILED(ref->Resolve(IID_PPV_ARGS(&sink))) || !sink)
            continue;

        if (FAILED(sink->OnEvent(eventBstr.get())))
        {
            // Client disconnected — clear the matching sink reference.
            std::lock_guard lock{ s_instancesMutex };
            for (auto* instance : s_instances)
            {
                std::lock_guard cbLock{ instance->_callbackMutex };
                if (instance->_sinkRef.Get() == ref.Get())
                {
                    instance->_sinkRef.Reset();
                    break;
                }
            }
        }
    }
}

// ============================================================================
// ITerminalProtocol — Meta
// ============================================================================

STDMETHODIMP TerminalProtocolComServer::Authenticate(BSTR /*token*/, BSTR* resultJson)
try
{
    RETURN_HR_IF_NULL(E_POINTER, resultJson);
    *resultJson = nullptr;
    RETURN_HR_IF(E_NOT_VALID_STATE, !s_emperor);

    // DEV BYPASS: always authenticate — credential plumbing not yet implemented.
    _authenticated = true;

    // Register for event delivery on successful authentication.
    _addInstance();

    Json::Value v;
    v["authenticated"] = _authenticated;
    // 2.2 — SendInput restored on the COM surface; pane identifiers remain GUIDs.
    v["protocol_version"] = "2.2";
    *resultJson = _bstrFromJson(v);
    return S_OK;
}
CATCH_RETURN()

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
        info.TabCount = logic.TabCount();
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

// ============================================================================
// Events — push-based via classic sink
// ============================================================================

STDMETHODIMP TerminalProtocolComServer::Subscribe(ITerminalProtocolEventSink* sink)
try
{
    RETURN_HR_IF(E_INVALIDARG, !sink);
    RETURN_HR_IF(E_ACCESSDENIED, !_authenticated);

    // Store the sink as an agile reference so it can be resolved + called from
    // any apartment (the VT-event fan-out runs on the UI/STA thread, while the
    // sink was unmarshaled on an MTA thread).
    ComPtr<IAgileReference> ref;
    RETURN_IF_FAILED(::RoGetAgileReference(AGILEREFERENCE_DEFAULT, __uuidof(ITerminalProtocolEventSink), sink, &ref));

    {
        std::lock_guard lock{ _callbackMutex };
        _sinkRef = ref;
    }

    // Ensure page events are wired up (one-time global init).
    _ensurePageEventsRegistered();
    return S_OK;
}
CATCH_RETURN()

STDMETHODIMP TerminalProtocolComServer::Unsubscribe()
{
    std::lock_guard lock{ _callbackMutex };
    _sinkRef.Reset();
    return S_OK;
}

STDMETHODIMP TerminalProtocolComServer::SendEvent(BSTR eventJson)
try
{
    RETURN_HR_IF(E_ACCESSDENIED, !_authenticated);

    const auto eventH = _hstr(eventJson);
    auto jsonStr = winrt::to_string(eventH);
    Json::Value evt;
    const auto route = ProtocolParsing::ClassifySendEvent(jsonStr, evt);
    RETURN_HR_IF(E_INVALIDARG, route == ProtocolParsing::SendEventRoute::Invalid);

    switch (route)
    {
    case ProtocolParsing::SendEventRoute::AutofixState:
        _dispatchAutofixStateToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::AgentStatus:
        _dispatchAgentStatusToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::CloseAgentPane:
        // User pressed Ctrl+C twice in the wta TUI. Marshal to the UI
        // thread; the page-side handler resolves the tab via `tab_id`
        // and tears down that tab's agent pane.
        _dispatchCloseAgentPaneToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::AgentState:
        // Per-tab agent-pane UI snapshot from wta. Page-side handler
        // routes by `tab_id` to the matching AgentPaneContent (creating
        // or tearing down the pane on that tab as needed).
        _dispatchAgentStateChangedToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::ResumeInNewAgentTab:
        // Session view's Shift+Enter handler in the wta TUI. WT creates
        // a new tab and asks wta to open an agent pane in it.
        _dispatchResumeInNewAgentTabToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::AgentChipTarget:
        // Helper override for which pane gets the "Agent" chip; null
        // pane_session_id reverts the tab to source-flag-driven chip.
        _dispatchAgentChipTargetToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::RestartAgentStack:
        // `/restart` from any agent pane TUI. Page-side handler tears
        // down every agent pane in its window and force-restarts the
        // wta-master process via SharedWta.
        _dispatchRestartAgentStackToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::RestartAgentPane:
        // Master detected a helper's pipe disconnect (crash or clean
        // exit). Page-side handler resolves the tab via `tab_id` and
        // re-warms a fresh helper, resuming `session_id`. Suppressed when
        // the pane was torn down deliberately (Ctrl+C×2, tab close).
        _dispatchRestartAgentPaneToPage(eventH);
        return S_OK;
    case ProtocolParsing::SendEventRoute::Broadcast:
    {
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        s_NotifyEventToComClients(Json::writeString(wb, evt));
        return S_OK;
    }
    default:
        return S_OK;
    }
}
CATCH_RETURN()

// Agent-pane dispatch methods — stubbed out (agent pane infrastructure not ported).
void TerminalProtocolComServer::_dispatchAutofixStateToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchAgentStatusToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchCloseAgentPaneToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchAgentStateChangedToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchResumeInNewAgentTabToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchAgentChipTargetToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchRestartAgentStackToPage(const winrt::hstring& /*eventJson*/) {}
void TerminalProtocolComServer::_dispatchRestartAgentPaneToPage(const winrt::hstring& /*eventJson*/) {}


