// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>

#include "Formatting.h"

// Classic-COM Terminal protocol. Generated from
// src/host/proxy/ITerminalProtocol.idl; found via the OpenConsoleProxy IntDir
// added to this project's include path. Marshaled by the OpenConsoleProxy
// proxy/stub (NOT WinRT MBM), so activation/marshaling never hits the combase
// WinRT activation catalog.
#include "ITerminalProtocol.h"

#include <CLI/CLI.hpp>

#include <wil/resource.h>

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// ── Helpers ──

// Per-brand CLSID — matches the server's compile-time selection.
#if defined(WT_BRANDING_RELEASE)
static constexpr CLSID CLSID_TerminalProtocol = { 0x832FDEC7, 0xAA6F, 0x4BAB, { 0x85, 0xFA, 0xA4, 0x91, 0x40, 0x56, 0x38, 0xFC } };
#elif defined(WT_BRANDING_PREVIEW)
static constexpr CLSID CLSID_TerminalProtocol = { 0xD77C8A1A, 0x83C0, 0x42FC, { 0xBA, 0xDF, 0x9B, 0xE8, 0x2E, 0x2A, 0x16, 0x24 } };
#elif defined(WT_BRANDING_CANARY)
static constexpr CLSID CLSID_TerminalProtocol = { 0x264DE65B, 0xF597, 0x4183, { 0x8A, 0x76, 0x6A, 0x03, 0x9A, 0x60, 0x47, 0x25 } };
#else
static constexpr CLSID CLSID_TerminalProtocol = { 0xAD9425AA, 0x1722, 0x4E7B, { 0xA4, 0x51, 0xAA, 0x1D, 0x09, 0x10, 0x6E, 0x83 } };
#endif

static winrt::com_ptr<ITerminalProtocol> ConnectToTerminal(std::string* outVersion = nullptr)
{
    winrt::com_ptr<ITerminalProtocol> server;
    auto hr = CoCreateInstance(CLSID_TerminalProtocol, nullptr, CLSCTX_LOCAL_SERVER, __uuidof(ITerminalProtocol), server.put_void());
    if (FAILED(hr))
    {
        fprintf(stderr, "[wtcli] Connection failed: 0x%08X\n", static_cast<uint32_t>(hr));
        return nullptr;
    }

    if (outVersion)
        *outVersion = "2.2";
    return server;
}

// Call a method that returns a JSON BSTR; parse into `out`. Returns the HRESULT.
template<typename F>
static HRESULT CallJson(F&& call, Json::Value& out)
{
    BSTR raw = nullptr;
    HRESULT hr = call(&raw);
    if (SUCCEEDED(hr))
    {
        bool parsed = false;
        if (raw)
        {
            Json::CharReaderBuilder rb;
            std::string errs;
            auto s = winrt::to_string(winrt::hstring{ raw });
            std::istringstream ss(s);
            parsed = Json::parseFromStream(rb, ss, &out, &errs);
        }
        // A success HRESULT with a null BSTR or malformed JSON is a broken
        // server contract; surface it as an error so callers' FAILED(hr)
        // checks fire immediately instead of proceeding with a
        // default-constructed `out`.
        if (!parsed)
            hr = E_UNEXPECTED;
    }
    if (raw)
        SysFreeString(raw);
    return hr;
}

static std::string GuidToString(const GUID& g)
{
    wchar_t buf[40]{};
    StringFromGUID2(g, buf, ARRAYSIZE(buf));
    std::wstring ws(buf);
    if (ws.size() > 2 && ws.front() == L'{' && ws.back() == L'}')
        ws = ws.substr(1, ws.size() - 2);
    return winrt::to_string(winrt::hstring{ ws });
}

static GUID GuidFromString(const std::string& target)
{
    auto wstr = winrt::to_hstring(target);
    std::wstring guidStr{ wstr };
    if (!guidStr.empty() && guidStr[0] != L'{')
        guidStr = L"{" + guidStr + L"}";
    GUID g{};
    if (FAILED(CLSIDFromString(guidStr.c_str(), &g)))
    {
        if (!target.empty())
            fprintf(stderr, "[wtcli] Invalid session ID: %s\n", target.c_str());
        return GUID{};
    }
    return g;
}

// Resolve a session id: an explicit GUID string, or the active pane's id.
static GUID ResolveSessionId(ITerminalProtocol* server, const std::string& target)
{
    if (!target.empty())
        return GuidFromString(target);

    Json::Value info;
    const auto hr = CallJson([&](BSTR* j) { return server->GetActivePane(j); }, info);
    if (FAILED(hr))
    {
        fprintf(stderr, "[wtcli] Could not resolve active pane (GetActivePane failed: 0x%08X)\n", static_cast<uint32_t>(hr));
        return GUID{};
    }
    const auto sessionId = info["session_id"].asString();
    if (sessionId.empty())
    {
        fprintf(stderr, "[wtcli] No active pane.\n");
        return GUID{};
    }
    return GuidFromString(sessionId);
}

static uint64_t GetFirstWindowId(ITerminalProtocol* server)
{
    Json::Value windows;
    CallJson([&](BSTR* j) { return server->ListWindows(j); }, windows);
    if (windows.isArray() && !windows.empty())
        return windows[0u]["window_id"].asUInt64();
    return 0;
}

static uint32_t GetFirstTabId(ITerminalProtocol* server, uint64_t windowId)
{
    Json::Value tabs;
    CallJson([&](BSTR* j) { return server->ListTabs(windowId, j); }, tabs);
    if (tabs.isArray() && !tabs.empty())
        return tabs[0u]["tab_id"].asUInt();
    return UINT32_MAX;
}

// Allocate a BSTR from a UTF-8 std::string.
static BSTR Bstr(const std::string& s)
{
    return SysAllocString(winrt::to_hstring(s).c_str());
}

// Parse a base-10 unsigned 64-bit integer without throwing (unlike std::stoull,
// which aborts wtcli on non-numeric input). Returns false on empty, non-numeric,
// trailing-garbage, or overflowing input.
static bool TryParseU64(const std::string& s, uint64_t& out)
{
    if (s.empty())
        return false;
    uint64_t v = 0;
    const auto* first = s.data();
    const auto* last = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(first, last, v);
    if (ec != std::errc{} || ptr != last)
        return false;
    out = v;
    return true;
}

// ── Main ──

int main()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    CLI::App app{ "wtcli - Windows Terminal CLI" };
    app.require_subcommand(0, 1);

    bool jsonMode = false;
    int exitCode = 0;
    app.add_flag("--json", jsonMode, "Output raw JSON");

    auto connect = [&]() -> winrt::com_ptr<ITerminalProtocol> {
        auto server = ConnectToTerminal();
        if (!server)
            exitCode = 1;
        return server;
    };

    // ── list-windows ──
    auto* listWindowsCmd = app.add_subcommand("list-windows", "List all windows")->alias("lsw");
    listWindowsCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        Json::Value windows;
        auto hr = CallJson([&](BSTR* j) { return server->ListWindows(j); }, windows);
        if (FAILED(hr)) { fprintf(stderr, "ListWindows failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
        {
            Json::Value arr(Json::objectValue);
            arr["windows"] = windows;
            PrintJson(arr);
        }
        else
        {
            FormatWindowsHuman(windows);
        }
    });

    // ── list-tabs ──
    std::string listTabsWindowId;
    auto* listTabsCmd = app.add_subcommand("list-tabs", "List tabs in a window")->alias("lst");
    listTabsCmd->add_option("-w,--window-id", listTabsWindowId, "Window ID");
    listTabsCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        uint64_t wid = 0;
        if (listTabsWindowId.empty())
        {
            wid = GetFirstWindowId(server.get());
            if (wid == 0)
            {
                // 0 is the server's "no filter" sentinel, not a real window id;
                // bail rather than silently listing tabs for ALL windows.
                fprintf(stderr, "[wtcli] Could not resolve a window (no windows or ListWindows failed)\n");
                exitCode = 1;
                return;
            }
        }
        else if (!TryParseU64(listTabsWindowId, wid))
        {
            fprintf(stderr, "[wtcli] Invalid --window-id: %s\n", listTabsWindowId.c_str());
            exitCode = 1;
            return;
        }
        Json::Value tabs;
        auto hr = CallJson([&](BSTR* j) { return server->ListTabs(wid, j); }, tabs);
        if (FAILED(hr)) { fprintf(stderr, "ListTabs failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
        {
            Json::Value arr(Json::objectValue);
            arr["tabs"] = tabs;
            PrintJson(arr);
        }
        else
        {
            FormatTabsHuman(tabs);
        }
    });

    // ── list-panes ──
    std::string listPanesTabId, listPanesWindowId;
    auto* listPanesCmd = app.add_subcommand("list-panes", "List panes in a tab")->alias("lsp");
    listPanesCmd->add_option("-t,--tab-id", listPanesTabId, "Tab ID");
    listPanesCmd->add_option("-w,--window-id", listPanesWindowId, "Window ID");
    listPanesCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        uint64_t wid = 0;
        if (!listPanesWindowId.empty() && !TryParseU64(listPanesWindowId, wid))
        {
            fprintf(stderr, "[wtcli] Invalid --window-id: %s\n", listPanesWindowId.c_str());
            exitCode = 1;
            return;
        }
        uint32_t tid = UINT32_MAX;
        if (!listPanesTabId.empty())
        {
            uint64_t t = 0;
            if (!TryParseU64(listPanesTabId, t) || t > UINT32_MAX)
            {
                fprintf(stderr, "[wtcli] Invalid --tab-id: %s\n", listPanesTabId.c_str());
                exitCode = 1;
                return;
            }
            tid = static_cast<uint32_t>(t);
        }
        if (tid == UINT32_MAX)
        {
            if (wid == 0)
            {
                wid = GetFirstWindowId(server.get());
                if (wid == 0)
                {
                    fprintf(stderr, "[wtcli] Could not resolve a window (no windows or ListWindows failed)\n");
                    exitCode = 1;
                    return;
                }
            }
            tid = GetFirstTabId(server.get(), wid);
            if (tid == UINT32_MAX)
            {
                fprintf(stderr, "[wtcli] Could not resolve a tab (no tabs or ListTabs failed)\n");
                exitCode = 1;
                return;
            }
        }
        Json::Value panes;
        auto hr = CallJson([&](BSTR* j) { return server->ListPanes(wid, tid, j); }, panes);
        if (FAILED(hr)) { fprintf(stderr, "ListPanes failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
        {
            Json::Value arr(Json::objectValue);
            arr["panes"] = panes;
            PrintJson(arr);
        }
        else
        {
            FormatPanesHuman(panes);
        }
    });

    // ── active-pane ──
    auto* activePaneCmd = app.add_subcommand("active-pane", "Show the currently active pane");
    activePaneCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        Json::Value info;
        auto hr = CallJson([&](BSTR* j) { return server->GetActivePane(j); }, info);
        if (FAILED(hr)) { fprintf(stderr, "GetActivePane failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
            PrintJson(info);
        else
            FormatActivePaneHuman(info);
    });

    // ── capture-pane ──
    std::string capturePaneTarget;
    int captureMaxLines = 200;
    bool captureLastPrompt = false;
    auto* capturePaneCmd = app.add_subcommand("capture-pane", "Capture pane output")->alias("capturep");
    capturePaneCmd->add_option("-t,--target", capturePaneTarget, "Session ID (GUID)");
    capturePaneCmd->add_option("-l,--max-lines", captureMaxLines, "Max lines");
    capturePaneCmd->add_flag("--last-prompt", captureLastPrompt,
        "Only return the most recent completed shell prompt (command + output, requires OSC 133 shell integration)");
    capturePaneCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        auto sessionId = ResolveSessionId(server.get(), capturePaneTarget);
        wil::unique_bstr src{ Bstr(captureLastPrompt ? "last_prompt" : "scrollback") };
        Json::Value output;
        auto hr = CallJson([&](BSTR* j) { return server->ReadPaneOutput(sessionId, src.get(), captureMaxLines, j); }, output);
        if (FAILED(hr)) { fprintf(stderr, "ReadPaneOutput failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
            PrintJson(output);
        else
            printf("%s\n", output["content"].asString().c_str());
    });

    // ── pane-status ──
    std::string paneStatusTarget;
    auto* paneStatusCmd = app.add_subcommand("pane-status", "Show pane process status");
    paneStatusCmd->add_option("-t,--target", paneStatusTarget, "Session ID (GUID)");
    paneStatusCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        auto sessionId = ResolveSessionId(server.get(), paneStatusTarget);
        Json::Value status;
        auto hr = CallJson([&](BSTR* j) { return server->GetProcessStatus(sessionId, j); }, status);
        if (FAILED(hr)) { fprintf(stderr, "GetProcessStatus failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
            PrintJson(status);
        else
            FormatPaneStatusHuman(status);
    });

    // ── new-tab ──
    std::string newTabCommand, newTabTitle, newTabCwd;
    auto* newTabCmd = app.add_subcommand("new-tab", "Create a new tab")->alias("neww");
    newTabCmd->add_option("-c,--command", newTabCommand, "Command to run");
    newTabCmd->add_option("-n,--title", newTabTitle, "Tab title");
    newTabCmd->add_option("-d,--cwd", newTabCwd, "Starting directory");
    newTabCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        wil::unique_bstr profile{ Bstr("") }, command{ Bstr(newTabCommand) }, title{ Bstr(newTabTitle) }, cwd{ Bstr(newTabCwd) };
        Json::Value result;
        auto hr = CallJson([&](BSTR* j) {
            return server->CreateTab(0, profile.get(), command.get(), title.get(), cwd.get(), false, true, j);
        }, result);
        if (FAILED(hr)) { fprintf(stderr, "CreateTab failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
            PrintJson(result);
        else
            FormatCreatedTabHuman(result);
    });

    // ── split-pane ──
    std::string splitPaneTarget, splitPaneCommand, splitPaneDirection;
    bool splitHorizontal = false, splitVertical = false;
    double splitSize = 0.5;
    auto* splitPaneCmd = app.add_subcommand("split-pane", "Split a pane")->alias("splitw");
    splitPaneCmd->add_option("-t,--target", splitPaneTarget, "Session ID (GUID)");
    splitPaneCmd->add_option("-d,--direction", splitPaneDirection, "Split direction: right|left|up|down|auto");
    splitPaneCmd->add_flag("-H,--horizontal", splitHorizontal, "Split horizontally (legacy alias for --direction down)");
    splitPaneCmd->add_flag("-v,--vertical", splitVertical, "Split vertically (legacy alias for --direction right)");
    splitPaneCmd->add_option("-s,--size", splitSize, "Size fraction");
    splitPaneCmd->add_option("-c,--command", splitPaneCommand, "Command to run");
    splitPaneCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        auto sessionId = ResolveSessionId(server.get(), splitPaneTarget);
        std::string dir;
        if (!splitPaneDirection.empty())
            dir = splitPaneDirection;
        else if (splitHorizontal)
            dir = "down";
        else if (splitVertical)
            dir = "right";
        else
            dir = "automatic";
        wil::unique_bstr dirB{ Bstr(dir) }, profile{ Bstr("") }, command{ Bstr(splitPaneCommand) };
        Json::Value result;
        auto hr = CallJson([&](BSTR* j) {
            return server->SplitPane(sessionId, dirB.get(), static_cast<float>(splitSize), profile.get(), command.get(), true, j);
        }, result);
        if (FAILED(hr)) { fprintf(stderr, "SplitPane failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
            PrintJson(result);
        else
            FormatCreatedPaneHuman(result);
    });

    // ── kill-pane ──
    std::string killPaneTarget;
    auto* killPaneCmd = app.add_subcommand("kill-pane", "Close a pane")->alias("killp");
    killPaneCmd->add_option("-t,--target", killPaneTarget, "Session ID (GUID)");
    killPaneCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        auto sessionId = ResolveSessionId(server.get(), killPaneTarget);
        auto hr = server->ClosePane(sessionId);
        if (FAILED(hr)) { fprintf(stderr, "ClosePane failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
        {
            Json::Value v;
            v["ok"] = true;
            v["session_id"] = GuidToString(sessionId);
            PrintJson(v);
        }
        else
        {
            printf("Session %s closed.\n", GuidToString(sessionId).c_str());
        }
    });

    // ── focus-pane ──
    std::string focusPaneTarget;
    auto* focusPaneCmd = app.add_subcommand("focus-pane", "Switch focus to a pane")->alias("focusp");
    focusPaneCmd->add_option("-t,--target", focusPaneTarget, "Session ID (GUID)");
    focusPaneCmd->callback([&]() {
        auto server = connect();
        if (!server) return;
        auto sessionId = ResolveSessionId(server.get(), focusPaneTarget);
        auto hr = server->FocusPane(sessionId);
        if (FAILED(hr)) { fprintf(stderr, "FocusPane failed: 0x%08X\n", static_cast<uint32_t>(hr)); exitCode = 1; return; }
        if (jsonMode)
        {
            Json::Value v;
            v["ok"] = true;
            v["session_id"] = GuidToString(sessionId);
            PrintJson(v);
        }
        else
        {
            printf("Focused pane %s.\n", GuidToString(sessionId).c_str());
        }
    });

    // ── Default (no subcommand) ──
    app.callback([&]() {
        if (app.get_subcommands().empty())
        {
            printf("wtcli - Windows Terminal CLI\n\n");
            printf("Usage: wtcli [--json] <subcommand>\n\n");
            printf("Run 'wtcli --help' for available subcommands.\n");
        }
    });

    try
    {
        app.parse(__argc, __argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    return exitCode;
}
