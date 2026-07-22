// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// This file contains the protocol bridge methods for TerminalPage.
// These methods are called by the TerminalProtocolComServer to query
// and mutate terminal state.
//
// IMPORTANT: These methods are called from background threads (COM).
// All access to UI state must be marshaled to the UI thread via Dispatcher().
// Each method is a direct coroutine that uses co_await to switch threads.
// The ComServer calls .get() on the returned IAsyncOperation to block.

#include "pch.h"
#include "TerminalPage.h"
#include "../../types/inc/utils.hpp"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"

#include <wil/resource.h>
#include "../TerminalProtocol/ProtocolParsing.h"

namespace ProtocolParsing = Microsoft::Terminal::Protocol::Parsing;

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings::Model;
namespace Protocol = winrt::Microsoft::Terminal::Protocol;

namespace winrt::TerminalApp::implementation
{
    // Helper to get PID from a pane's terminal control connection.
    static uint32_t _getPidFromPane(const std::shared_ptr<Pane>& pane)
    {
        if (const auto termControl = pane->GetTerminalControl())
        {
            const auto conn = termControl.Connection();
            if (conn)
            {
                if (const auto conpty = conn.try_as<ConptyConnection>())
                {
                    const auto handle = conpty.RootProcessHandle();
                    if (handle)
                    {
                        return static_cast<uint32_t>(GetProcessId(reinterpret_cast<HANDLE>(handle)));
                    }
                }
            }
        }
        return 0;
    }

    // Get the connection SessionId for a terminal pane, or empty guid for non-terminal panes.
    static winrt::guid _getSessionIdFromPane(const std::shared_ptr<Pane>& pane)
    {
        if (const auto termContent = pane->GetContent().try_as<TerminalApp::TerminalPaneContent>())
        {
            if (const auto control = termContent.GetTermControl())
            {
                if (const auto conn = control.Connection())
                {
                    return conn.SessionId();
                }
            }
        }
        return {};
    }

    // Find a pane by SessionId across all tabs using WalkTree.
    static std::shared_ptr<Pane> _findPaneBySessionId(const std::shared_ptr<Pane>& rootPane, winrt::guid sessionId)
    {
        std::shared_ptr<Pane> found;
        rootPane->WalkTree([&](const auto& pane) {
            if (_getSessionIdFromPane(pane) == sessionId)
                found = pane;
        });
        return found;
    }

    uint32_t TerminalPage::TabCount() const
    {
        return [this]() -> IAsyncOperation<uint32_t> {
            co_await wil::resume_foreground(Dispatcher());
            co_return NumberOfTabs();
        }().get();
    }

    Windows::Foundation::IReference<uint32_t> TerminalPage::FocusedTabIndex() const
    {
        return [this]() -> IAsyncOperation<Windows::Foundation::IReference<uint32_t>> {
            co_await wil::resume_foreground(Dispatcher());
            const auto idx = _GetFocusedTabIndex();
            if (idx.has_value())
            {
                co_return Windows::Foundation::IReference<uint32_t>(idx.value());
            }
            co_return nullptr;
        }().get();
    }

    // ============================================================================
    // Queries — return typed WinRT structs
    // ============================================================================

    IAsyncOperation<Protocol::PaneInfo> TerminalPage::GetProtocolActivePane()
    {
        auto strong = get_strong();
        co_await wil::resume_foreground(Dispatcher());

        Protocol::PaneInfo result{};

        const auto focusedTabIdx = _GetFocusedTabIndex();
        if (!focusedTabIdx.has_value())
            co_return result;

        const auto tab = _tabs.GetAt(focusedTabIdx.value());
        const auto tabImpl = _GetTabImpl(tab);
        if (!tabImpl)
            co_return result;

        const auto activePane = tabImpl->GetActivePane();
        if (!activePane)
            co_return result;

        result.SessionId = _getSessionIdFromPane(activePane);
        result.TabId = focusedTabIdx.value();
        result.IsActive = true;
        result.IsAgentPane = false;

        if (const auto termContent = activePane->GetContent().try_as<TerminalApp::TerminalPaneContent>())
        {
            result.Title = termContent.Title();
            const auto profile = termContent.GetProfile();
            result.Profile = profile ? profile.Name() : L"";
        }

        if (const auto termControl = activePane->GetTerminalControl())
        {
            result.Cwd = termControl.WorkingDirectory();
        }

        result.Pid = _getPidFromPane(activePane);
        co_return result;
    }

    IAsyncOperation<Windows::Foundation::Collections::IVector<Protocol::TabInfo>> TerminalPage::GetProtocolTabs()
    {
        auto strong = get_strong();
        co_await wil::resume_foreground(Dispatcher());

        auto tabs = winrt::single_threaded_vector<Protocol::TabInfo>();
        const auto focusedIdx = _GetFocusedTabIndex();

        for (uint32_t i = 0; i < _tabs.Size(); ++i)
        {
            const auto tab = _tabs.GetAt(i);
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            Protocol::TabInfo info{};
            info.TabId = i;
            info.Title = tab.Title();
            info.IsActive = focusedIdx.has_value() && (focusedIdx.value() == i);
            // Count terminal panes only (those with a SessionId).
            uint32_t terminalPaneCount = 0;
            if (const auto rootPane = tabImpl->GetRootPane())
            {
                rootPane->WalkTree([&](const auto& pane) {
                    if (_getSessionIdFromPane(pane) != winrt::guid{})
                        terminalPaneCount++;
                });
            }
            info.PaneCount = terminalPaneCount;
            tabs.Append(info);
        }

        co_return tabs;
    }

    IAsyncOperation<Windows::Foundation::Collections::IVector<Protocol::PaneInfo>> TerminalPage::GetProtocolPanes(uint32_t tabIdFilter)
    {
        auto strong = get_strong();

        co_await wil::resume_foreground(Dispatcher());

        auto panes = winrt::single_threaded_vector<Protocol::PaneInfo>();

        for (uint32_t tabIdx = 0; tabIdx < _tabs.Size(); ++tabIdx)
        {
            if (tabIdFilter != UINT32_MAX && tabIdx != tabIdFilter)
                continue;

            const auto tab = _tabs.GetAt(tabIdx);
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto activePane = tabImpl->GetActivePane();

            rootPane->WalkTree([&](const auto& pane) {
                if (!pane->GetContent())
                    return; // Skip branch nodes

                const auto sid = _getSessionIdFromPane(pane);
                if (sid == winrt::guid{})
                    return; // Skip non-terminal panes

                Protocol::PaneInfo info{};
                info.SessionId = sid;
                info.TabId = tabIdx;
                info.IsAgentPane = false;
                info.IsActive = (activePane == pane);
                info.Pid = _getPidFromPane(pane);

                if (const auto termContent = pane->GetContent().try_as<TerminalApp::TerminalPaneContent>())
                {
                    info.Title = termContent.Title();
                    const auto profile = termContent.GetProfile();
                    info.Profile = profile ? profile.Name() : L"";

                    if (const auto termControl = pane->GetTerminalControl())
                    {
                        info.Rows = termControl.ViewHeight();
                        info.Columns = 0;
                        info.Cwd = termControl.WorkingDirectory();
                    }
                }

                panes.Append(info);
            });
        }

        co_return panes;
    }

    IAsyncOperation<Protocol::PaneOutput> TerminalPage::ReadProtocolPaneOutput(winrt::guid sessionId, hstring source, int32_t maxLines)
    {
        auto strong = get_strong();
        const auto sourceStr = winrt::to_string(source);
        const auto sourceRoute = ProtocolParsing::ClassifyPaneOutputSource(sourceStr);
        const auto effectiveMaxLines = (maxLines <= 0) ? 200 : maxLines;

        co_await wil::resume_foreground(Dispatcher());

        Protocol::PaneOutput result{};

        // UI-thread work: find pane, read buffer.
        hstring fullBuffer;
        int32_t viewHeight = 0;
        for (const auto& tab : _tabs)
        {
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            const auto termControl = foundPane->GetTerminalControl();
            if (!termControl)
                co_return result; // empty SessionId signals not-ready

            try
            {
                if (sourceRoute == ProtocolParsing::PaneOutputSource::LastPrompt)
                {
                    result.SessionId = sessionId;
                    const auto lastPrompt = termControl.ReadLastPrompt();
                    auto lastPromptStr = winrt::to_string(lastPrompt);
                    if (lastPromptStr.empty())
                    {
                        result.HasMarks = false;
                        result.Content = L"";
                        result.LineCount = 0;
                        result.Truncated = false;
                        co_return result;
                    }
                    int32_t lineCount = 1;
                    for (auto ch : lastPromptStr)
                    {
                        if (ch == '\n')
                            ++lineCount;
                    }
                    result.HasMarks = true;
                    result.Content = winrt::to_hstring(lastPromptStr);
                    result.LineCount = lineCount;
                    result.Truncated = false;
                    co_return result;
                }

                fullBuffer = termControl.ReadEntireBuffer();
                viewHeight = termControl.ViewHeight();
            }
            catch (...)
            {
                co_return result; // empty SessionId signals error
            }

            result.SessionId = sessionId;
            break;
        }

        if (result.SessionId == winrt::guid{})
            co_return result; // not found

        // Move off UI thread for string processing.
        co_await winrt::resume_background();

        auto fullBufferStr = winrt::to_string(fullBuffer);
        std::vector<std::string> lines;
        std::istringstream iss(fullBufferStr);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }

        if (sourceRoute == ProtocolParsing::PaneOutputSource::Screen)
        {
            const auto startIdx = lines.size() > static_cast<size_t>(viewHeight)
                                      ? lines.size() - viewHeight
                                      : 0;

            std::string content;
            int lineCount = 0;
            for (size_t i = startIdx; i < lines.size(); ++i)
            {
                if (!content.empty())
                    content += "\n";
                content += lines[i];
                lineCount++;
            }

            result.Content = winrt::to_hstring(content);
            result.LineCount = lineCount;
            result.Truncated = false;
        }
        else
        {
            const auto truncated = (static_cast<int32_t>(lines.size()) > effectiveMaxLines);
            const auto startIdx = truncated ? lines.size() - effectiveMaxLines : 0;

            std::string content;
            int lineCount = 0;
            for (size_t i = startIdx; i < lines.size(); ++i)
            {
                if (!content.empty())
                    content += "\n";
                content += lines[i];
                lineCount++;
            }

            result.Content = winrt::to_hstring(content);
            result.LineCount = lineCount;
            result.Truncated = truncated;
        }

        co_return result;
    }

    IAsyncOperation<Protocol::ProcessStatus> TerminalPage::GetProtocolProcessStatus(winrt::guid sessionId)
    {
        auto strong = get_strong();

        co_await wil::resume_foreground(Dispatcher());

        Protocol::ProcessStatus result{};

        for (const auto& tab : _tabs)
        {
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            result.SessionId = sessionId;

            const auto termControl = foundPane->GetTerminalControl();
            if (!termControl)
            {
                result.State = L"unknown";
                co_return result;
            }

            const auto conn = termControl.Connection();
            if (!conn)
            {
                result.State = L"exited";
                co_return result;
            }

            const auto connState = termControl.ConnectionState();

            if (connState == ConnectionState::Connected)
            {
                result.State = L"running";
                result.Pid = _getPidFromPane(foundPane);
            }
            else
            {
                result.State = L"exited";
                if (const auto conpty = conn.try_as<ConptyConnection>())
                {
                    const auto handle = conpty.RootProcessHandle();
                    if (handle)
                    {
                        DWORD exitCode = 0;
                        if (GetExitCodeProcess(reinterpret_cast<HANDLE>(handle), &exitCode))
                        {
                            if (exitCode != STILL_ACTIVE)
                            {
                                result.ExitCode = static_cast<int32_t>(exitCode);
                                result.HasExitCode = true;
                            }
                        }
                        result.Pid = static_cast<uint32_t>(GetProcessId(reinterpret_cast<HANDLE>(handle)));
                    }
                }
            }

            co_return result;
        }

        co_return result; // empty SessionId = not found
    }

    IAsyncOperation<Protocol::SessionVariable> TerminalPage::GetProtocolSessionVariable(winrt::guid /*sessionId*/, hstring /*name*/)
    {
        // Session variables require per-pane storage not yet ported.
        co_return Protocol::SessionVariable{};
    }

    // ============================================================================
    // Mutations — return typed structs or bool
    // ============================================================================

    IAsyncOperation<bool> TerminalPage::SetProtocolSessionVariable(winrt::guid /*sessionId*/, hstring /*name*/, hstring /*value*/)
    {
        // Session variables require per-pane storage not yet ported.
        co_return false;
    }

    IAsyncOperation<Protocol::TabCreationResult> TerminalPage::CreateProtocolTab(NewTerminalArgs args, bool background)
    {
        auto strong = get_strong();
        co_await wil::resume_foreground(Dispatcher());

        Protocol::TabCreationResult result{};

        auto pane = _MakePane(args, nullptr);
        if (!pane)
            co_return result;

        _CreateNewTabFromPane(pane, static_cast<uint32_t>(-1));
        _tabContent.UpdateLayout(); // Force synchronous terminal initialization

        if (_tabs.Size() == 0)
            co_return result;

        const auto newTabIdx = _tabs.Size() - 1;
        const auto newTab = _tabs.GetAt(newTabIdx);
        const auto tabImpl = _GetTabImpl(newTab);

        result.TabId = newTabIdx;

        if (tabImpl)
        {
            const auto rootPane = tabImpl->GetRootPane();
            if (rootPane)
            {
                result.SessionId = _getSessionIdFromPane(rootPane);
                result.Pid = _getPidFromPane(rootPane);
            }
        }

        co_return result;
    }

    IAsyncOperation<Protocol::TabCreationResult> TerminalPage::SplitProtocolPane(winrt::guid sessionId, SplitDirection direction, float size, NewTerminalArgs args, bool background)
    {
        auto strong = get_strong();

        co_await wil::resume_foreground(Dispatcher());

        Protocol::TabCreationResult result{};

        for (uint32_t tabIdx = 0; tabIdx < _tabs.Size(); ++tabIdx)
        {
            const auto tab = _tabs.GetAt(tabIdx);
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            if (const auto id = foundPane->Id())
            {
                tabImpl->FocusPane(id.value());
            }

            auto newPane = _MakePane(args, nullptr);
            if (!newPane)
                co_return result;

            const auto newPanePid = _getPidFromPane(newPane);
            auto newPaneRef = newPane; // copy shared_ptr before move

            _SplitPane(tabImpl, direction, size, std::move(newPane));
            _tabContent.UpdateLayout(); // Force synchronous terminal initialization

            result.TabId = tabIdx;
            result.SessionId = _getSessionIdFromPane(newPaneRef);
            result.Pid = newPanePid;
            co_return result;
        }

        co_return result;
    }

    IAsyncOperation<bool> TerminalPage::CloseProtocolPane(winrt::guid sessionId)
    {
        auto strong = get_strong();

        co_await wil::resume_foreground(Dispatcher());

        for (const auto& tab : _tabs)
        {
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            foundPane->Close();
            co_return true;
        }

        co_return false;
    }

    IAsyncOperation<bool> TerminalPage::SendProtocolInput(winrt::guid sessionId, hstring text)
    {
        auto strong = get_strong();
        // Replace \n with \r — shells expect carriage return (Enter key)
        // rather than line feed to execute commands.
        std::wstring input{ text };
        std::replace(input.begin(), input.end(), L'\n', L'\r');

        co_await wil::resume_foreground(Dispatcher());

        for (const auto& tab : _tabs)
        {
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            const auto termControl = foundPane->GetTerminalControl();
            if (!termControl)
                co_return false;

            termControl.SendInput(winrt::hstring{ input });
            co_return true;
        }

        co_return false;
    }

    IAsyncOperation<bool> TerminalPage::FocusProtocolPane(winrt::guid sessionId)
    {
        auto strong = get_strong();

        co_await wil::resume_foreground(Dispatcher());

        for (const auto& tab : _tabs)
        {
            const auto tabImpl = _GetTabImpl(tab);
            if (!tabImpl)
                continue;

            const auto rootPane = tabImpl->GetRootPane();
            if (!rootPane)
                continue;

            const auto foundPane = _findPaneBySessionId(rootPane, sessionId);
            if (!foundPane)
                continue;

            const auto paneId = foundPane->Id();
            if (!paneId)
                co_return false;

            _SetFocusedTab(tab);

            if (!tabImpl->FocusPane(paneId.value()))
                co_return false;

            if (const auto termControl = foundPane->GetTerminalControl())
            {
                termControl.Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
            }
            co_return true;
        }

        co_return false;
    }

}
