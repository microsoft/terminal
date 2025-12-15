// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxControl.h"

#include <sstream>
#include <iostream>
#include <LibraryResources.h>

#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"
#include "TerminalPage.h"
#include "TabRowControl.h"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

static const float PaneBorderSize = 2;
static const int StaticMenuCount = 4; // "Separator" "Settings" "Command Palette" "About"

// Set this to 1 to enable debug logging
#define DEBUG 0

#if DEBUG
#define print_debug(s, ...) \
    OutputDebugStringW(fmt::format(FMT_COMPILE(L"TMUX " s) __VA_OPT__(, ) __VA_ARGS__).c_str());
#else
#define print_debug(s, ...)
#endif

static std::wstring_view split_line(std::wstring_view& remaining)
{
    auto lf = remaining.find(L'\n');
    lf = std::min(lf, remaining.size());

    // Trim any potential \r before the \n
    auto end = lf;
    if (end != 0 && remaining[end - 1] == L'\r')
    {
        --end;
    }

    const auto line = til::safe_slice_abs(remaining, 0, end);
    remaining = til::safe_slice_abs(remaining, lf + 1, std::wstring_view::npos);
    return line;
}

static std::wstring_view tokenize_field(std::wstring_view& remaining)
{
    const auto end = remaining.find(L' ');
    const auto field = til::safe_slice_abs(remaining, 0, end);
    const auto beg_next = remaining.find_first_not_of(L' ', end);
    remaining = til::safe_slice_abs(remaining, beg_next, std::wstring_view::npos);
    return field;
}

static std::optional<int64_t> tokenize_number(std::wstring_view& remaining)
{
    return til::parse_unsigned<uint32_t>(tokenize_field(remaining), 10);
}

enum class IdentifierType : wchar_t
{
    Invalid = 0,
    Session = '$',
    Window = '@',
    Pane = '%',
};

struct Identifier
{
    IdentifierType type;
    int64_t value;
};

static Identifier tokenize_identifier(std::wstring_view& remaining)
{
    Identifier result = {
        .type = IdentifierType::Invalid,
        .value = -1,
    };

    const auto field = tokenize_field(remaining);
    if (field.empty())
    {
        return result;
    }

    const auto type = field.front();
    switch (type)
    {
    case L'$':
    case L'@':
    case L'%':
        break;
    default:
        return result;
    }

    const auto id = til::parse_unsigned<uint32_t>(field.substr(1), 10);
    if (!id)
    {
        return result;
    }

    result.type = (IdentifierType)type;
    result.value = *id;
    return result;
}

namespace winrt::TerminalApp::implementation
{
    TmuxControl::TmuxControl(TerminalPage& page) :
        _page{ page }
    {
        _dispatcherQueue = DispatcherQueue::GetForCurrentThread();

        const auto newTabRun = Documents::Run();
        newTabRun.Text(RS_(L"NewTabRun/Text"));
        const auto newPaneRun = Documents::Run();
        newPaneRun.Text(RS_(L"NewPaneRun/Text"));

        const auto textBlock = Controls::TextBlock{};
        textBlock.Inlines().Append(newTabRun);
        textBlock.Inlines().Append(Documents::LineBreak{});
        textBlock.Inlines().Append(newPaneRun);

        Controls::ToolTipService::SetToolTip(_newTabMenu, box_value(textBlock));
        _newTabMenu.Text(RS_(L"NewTmuxControlTab/Text"));

        Controls::FontIcon newTabIcon;
        newTabIcon.Glyph(L"\xF714");
        newTabIcon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons,Segoe MDL2 Assets" });
        _newTabMenu.Icon(newTabIcon);

        _newTabMenu.Click([this](auto&&, auto&&) {
            _openNewTerminalViaDropdown();
        });
    }

    bool TmuxControl::AcquireSingleUseLock(winrt::Microsoft::Terminal::Control::TermControl control) noexcept
    {
        if (_inUse)
        {
            return false;
        }

        // NOTE: This is safe, because `.control` only gets accessed via FeedInput(),
        // when it receives %session-changed and after it transitioned to the UI thread.
        _control = std::move(control);
        return true;
    }

    void TmuxControl::FeedInput(std::wstring_view str)
    {
        if (str.empty())
        {
            return;
        }

        // Our LF search logic is unable to recognize the lone ESC character
        // (= ST = end) as its own line. Let's just special case it here.
        if (str.ends_with(L'\x1b'))
        {
            _parseLine(std::wstring{ L"\x1b" });
            return;
        }

        auto idx = str.find(L'\n');

        // If there's leftover partial line, append the new data to it first.
        if (!_lineBuffer.empty())
        {
            const auto line = til::safe_slice_abs(str, 0, idx);
            _lineBuffer.insert(_lineBuffer.end(), line.begin(), line.end());

            // If this still wasn't a full line, wait for more data.
            if (idx == std::wstring_view::npos)
            {
                return;
            }

            // Strip of any remaining CR. We already removed the LF after the find() call.
            if (!_lineBuffer.empty() && _lineBuffer.back() == L'\r')
            {
                _lineBuffer.pop_back();
            }

            _parseLine(std::move(_lineBuffer));
            _lineBuffer.clear();

            // Move past the line we just processed.
            str = til::safe_slice_abs(str, idx + 1, std::wstring_view::npos);
            idx = str.find(L'\n');
        }

        while (idx != std::wstring_view::npos)
        {
            // Strip of any CR in front of our LF.
            auto end = idx;
            if (end != 0 && str[end - 1] == L'\r')
            {
                --end;
            }

            const auto line = til::safe_slice_abs(str, 0, end);
            _parseLine(std::wstring{ line });

            str = til::safe_slice_abs(str, idx + 1, std::wstring_view::npos);
            idx = str.find(L'\n');
        }

        // If there's any leftover partial line, stash it for later.
        if (!str.empty())
        {
            _lineBuffer.append(str);
        }
    }

    bool TmuxControl::TabIsTmuxControl(const winrt::com_ptr<Tab>& tab)
    {
        assert(_dispatcherQueue.HasThreadAccess());

        if (!tab)
        {
            return false;
        }

        for (auto& t : _attachedWindows)
        {
            if (t.second->TabViewIndex() == tab->TabViewIndex())
            {
                return true;
            }
        }

        if (_controlTab->TabViewIndex() == tab->TabViewIndex())
        {
            return true;
        }

        return false;
    }

    void TmuxControl::SplitPane(const winrt::com_ptr<Tab>& tab, SplitDirection direction)
    {
        const auto contentWidth = static_cast<float>(_page._tabContent.ActualWidth());
        const auto contentHeight = static_cast<float>(_page._tabContent.ActualHeight());
        const winrt::Windows::Foundation::Size availableSpace{ contentWidth, contentHeight };

        if (tab == nullptr)
        {
            return;
        }

        const auto realSplitType = tab->PreCalculateCanSplit(direction, 0.5f, availableSpace);
        if (!realSplitType)
        {
            return;
        }

        switch (*realSplitType)
        {
        case SplitDirection::Right:
            _sendSplitPane(tab->GetActivePane(), SplitDirection::Right);
            break;
        case SplitDirection::Down:
            _sendSplitPane(tab->GetActivePane(), SplitDirection::Down);
            break;
        default:
            break;
        }
    }

    safe_void_coroutine TmuxControl::_parseLine(std::wstring line)
    {
        if (line.empty())
        {
            co_return;
        }

        const auto self = shared_from_this();
        co_await wil::resume_foreground(_dispatcherQueue);

        print_debug(L"<<< {}\n", line);

        std::wstring_view remaining{ line };
        const auto type = tokenize_field(remaining);

        // Are we inside a %begin ... %end block? Anything until %end or %error
        // is considered part of the output so this deserves special handling.
        if (_insideOutputBlock)
        {
            if (til::equals(type, L"%end"))
            {
                _handleResponse(std::move(_responseBuffer));
                _responseBuffer.clear();
                _insideOutputBlock = false;
            }
            else if (til::equals(type, L"%error"))
            {
                // In theory our commands should not result in errors.
                if (_state != State::Init)
                {
                    assert(false);
                }

                if (_control)
                {
                    _responseBuffer.append(L"\r\n");
                    _control.InjectTextAtCursor(_responseBuffer);
                }

                if (!_commandQueue.empty())
                {
                    _commandQueue.pop_front();
                }
                _responseBuffer.clear();
                _insideOutputBlock = false;
            }
            else
            {
                // Note that at this point `remaining` will not be the whole `line` anymore.
                if (_responseBuffer.empty())
                {
                    _responseBuffer = std::move(line);
                }
                else
                {
                    _responseBuffer.append(L"\r\n");
                    _responseBuffer.append(line);
                }
            }
        }
        // Otherwise, we check for the, presumably, most common output type first: %output.
        else if (til::equals(type, L"%output"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Pane)
            {
                _deliverOutputToPane(id.value, remaining);
            }
        }
        else if (til::equals(type, L"%begin"))
        {
            _insideOutputBlock = true;
        }
        else if (til::equals(type, L"%session-changed"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Session)
            {
                _handleSessionChanged(id.value);
            }
        }
        else if (til::equals(type, L"%window-add"))
        {
            // We'll handle the initial window discovery ourselves during %session-changed.
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _handleWindowAdd(id.value);
            }
        }
        else if (til::equals(type, L"%window-close"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _handleWindowClose(id.value);
            }
        }
        else if (til::equals(type, L"%window-pane-changed"))
        {
            const auto windowId = tokenize_identifier(remaining);
            const auto paneId = tokenize_identifier(remaining);

            if (windowId.type == IdentifierType::Window && paneId.type == IdentifierType::Pane)
            {
                _handleWindowPaneChanged(windowId.value, paneId.value);
            }
        }
        else if (til::equals(type, L"%window-renamed"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _handleWindowRenamed(id.value, winrt::hstring{ remaining });
            }
        }
        else if (til::equals(type, L"%layout-change"))
        {
            const auto windowId = tokenize_identifier(remaining);
            const auto layout = tokenize_field(remaining);

            if (windowId.type == IdentifierType::Window && !layout.empty())
            {
                _handleLayoutChange(windowId.value, layout);
            }
        }
        else if (til::equals(type, L"\033"))
        {
            _handleDetach();
        }
    }

    void TmuxControl::_handleAttach()
    {
        _state = State::Attaching;

        if (const auto terminalTab{ _page._GetFocusedTabImpl() })
        {
            if (const auto pane{ terminalTab->GetActivePane() })
            {
                _profile = pane->GetProfile();
            }
        }
        if (!_profile)
        {
            _profile = CascadiaSettings::LoadDefaults().ProfileDefaults();
        }

        // TODO: The CharacterDimensions may be non-default because the text is zoomed in.
        const auto fontSize = _control.CharacterDimensions();
        const auto width = _page.ActualWidth();
        const auto height = _page.ActualHeight();

        // Tmux use one character to draw separator line, so we have to make the padding
        // plus two borders equals one character's width or height
        // Same reason, we have to disable the scrollbar. Otherwise, the local panes size
        // will not match Tmux's.
        _fontWidth = fontSize.Width;
        _fontHeight = fontSize.Height;
        _thickness.Left = _thickness.Right = std::max(0.0f, (_fontWidth - 2 * PaneBorderSize) / 2);
        _thickness.Top = _thickness.Bottom = std::max(0.0f, (_fontHeight - 2 * PaneBorderSize) / 2);
        _terminalWidth = (til::CoordType)floor((width - _thickness.Left - _thickness.Right) / _fontWidth);
        _terminalHeight = (til::CoordType)floor((height - _thickness.Top - _thickness.Bottom) / _fontHeight);

        _profile.Padding(XamlThicknessToOptimalString(_thickness));
        _profile.ScrollState(winrt::Microsoft::Terminal::Control::ScrollbarState::Hidden);
        _profile.Icon(MediaResourceHelper::FromString(L"\uF714"));
        _profile.Name(L"TmuxTab");

        // Intercept the control terminal's input, ignore all user input, except 'q' as detach command.
        _detachKeyDownRevoker = _control.KeyDown([this](auto, auto& e) {
            if (e.Key() == VirtualKey::Q)
            {
                _sendIgnoreResponse(L"detach\n");
            }
            e.Handled(true);
        });

        _windowSizeChangedRevoker = _page.SizeChanged([this](auto, auto) {
            const auto fontSize = _control.CharacterDimensions();
            const auto width = _page.ActualWidth();
            const auto height = _page.ActualHeight();

            _fontWidth = fontSize.Width;
            _fontHeight = fontSize.Height;
            _thickness.Left = _thickness.Right = std::max(0.0f, (_fontWidth - 2 * PaneBorderSize) / 2);
            _thickness.Top = _thickness.Bottom = std::max(0.0f, (_fontHeight - 2 * PaneBorderSize) / 2);
            _terminalWidth = (til::CoordType)floor((width - _thickness.Left - _thickness.Right) / _fontWidth);
            _terminalHeight = (til::CoordType)floor((height - _thickness.Top - _thickness.Bottom) / _fontHeight);

            _sendSetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));

            for (auto& w : _attachedWindows)
            {
                _sendResizeWindow(w.first, _terminalWidth, _terminalHeight);
            }
        });

        // Dynamically insert the "Tmux Control Tab" menu item into flyout menu
        const auto tabRow = _page.TabRow();
        const auto tabRowImpl = winrt::get_self<TabRowControl>(tabRow);
        const auto newTabButton = tabRowImpl->NewTabButton();
        const auto flyout = newTabButton.Flyout().as<Controls::MenuFlyout>();
        const auto menuCount = flyout.Items().Size();
        flyout.Items().InsertAt(menuCount - StaticMenuCount, _newTabMenu);

        // Register new tab button click handler for tmux control
        _newTabClickRevoker = newTabButton.Click([this](auto&&, auto&&) {
            if (TabIsTmuxControl(_page._GetFocusedTabImpl()))
            {
                _openNewTerminalViaDropdown();
            }
        });

        _controlTab = _page._GetTabImpl(_page._GetFocusedTab());
        _control.InjectTextAtCursor(RS_(L"TmuxControlInfo"));
    }

    void TmuxControl::_handleDetach()
    {
        // WARNING: The Pane/AttachedPane destructors are highly non-trivial. Due to how they drop the potentially last
        // reference to TermControl, they may close the TmuxConnection, which in turn calls back into TmuxControl.
        // To make the destruction predictable, we extend their lifetime until after we cleared out everything.
        const auto attachedWindows = std::move(_attachedWindows);
        const auto attachedPanes = std::move(_attachedPanes);

        const auto control = std::move(_control);
        const auto detachKeyDownRevoker = std::move(_detachKeyDownRevoker);
        const auto windowSizeChangedRevoker = std::move(_windowSizeChangedRevoker);
        const auto newTabClickRevoker = std::move(_newTabClickRevoker);
        auto& page = _page;

        {
            _control = nullptr;
            _controlTab = nullptr;
            _profile = nullptr;
            _state = State::Init;
            _inUse = false;

            _lineBuffer.clear();
            _responseBuffer.clear();
            _insideOutputBlock = false;

            _detachKeyDownRevoker = {};
            _windowSizeChangedRevoker = {};
            _newTabClickRevoker = {};

            _commandQueue.clear();
            _attachedWindows.clear();
            _attachedPanes.clear();

            _sessionId = -1;
            _activePaneId = -1;
            _activeWindowId = -1;

            _terminalWidth = 0;
            _terminalHeight = 0;
            _thickness = {};
            _fontWidth = 0;
            _fontHeight = 0;

            _splittingPane = {};
        }

        // WARNING: If you see any class members being used past this point you're doing it wrong.
        // Move then into local variables first. This ensures that callbacks, etc., see the reset state already.

        for (auto& w : attachedWindows)
        {
            w.second->Close();
        }

        const auto tabRow = page.TabRow();
        const auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(tabRow);
        const auto newTabButton = tabRowImpl->NewTabButton();
        const auto newTabItems = newTabButton.Flyout().as<Controls::MenuFlyout>().Items();

        control.KeyDown(detachKeyDownRevoker);
        page.SizeChanged(windowSizeChangedRevoker);
        newTabButton.Click(newTabClickRevoker);

        // Remove the "Tmux Control Tab" menu item from flyout menu
        uint32_t i = 0;
        for (const auto& m : newTabItems)
        {
            if (const auto item = m.try_as<Controls::MenuFlyoutItem>(); item && item.Text() == RS_(L"NewTmuxControlTab/Text"))
            {
                newTabItems.RemoveAt(i);
                break;
            }
            i++;
        }
    }

    void TmuxControl::_handleSessionChanged(int64_t sessionId)
    {
        _sessionId = sessionId;
        _sendSetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));
        _sendDiscoverWindows(_sessionId);
    }

    void TmuxControl::_handleWindowAdd(int64_t windowId)
    {
        _sendDiscoverNewWindow(windowId);
    }

    void TmuxControl::_handleWindowRenamed(int64_t windowId, winrt::hstring name)
    {
        if (const auto tab = _getTab(windowId))
        {
            tab->SetTabText(std::move(name));
        }
    }

    void TmuxControl::_handleWindowClose(int64_t windowId)
    {
        std::erase_if(_attachedPanes, [windowId](const auto& pair) {
            return pair.second.windowId == windowId;
        });

        if (const auto nh = _attachedWindows.extract(windowId))
        {
            nh.mapped()->Close();
        }
    }

    void TmuxControl::_handleWindowPaneChanged(int64_t windowId, int64_t newPaneId)
    {
        const auto tab = _getTab(windowId);
        if (!tab)
        {
            return;
        }

        winrt::Microsoft::Terminal::Control::TermControl control{ nullptr };

        // TODO: The system of relying on _splittingPane to compute pane
        // splits and know which direction to split is highly fragile.
        if (_splittingPane.first)
        {
            const auto activePane = tab->GetActivePane();
            if (activePane.get() == _splittingPane.first.get())
            {
                auto [p, pane] = _newPane(windowId, newPaneId);
                control = p.control;
                tab->SplitPane(_splittingPane.second, 0.5f, std::move(pane));
            }

            _splittingPane.first = nullptr;
        }
        else
        {
            if (const auto it = _attachedPanes.find(newPaneId); it != _attachedPanes.end())
            {
                control = it->second.control;
            }
        }

        if (control)
        {
            control.Focus(FocusState::Programmatic);
        }
    }

    // TODO: How do we reconcile an arbitrary layout change?
    void TmuxControl::_handleLayoutChange(int64_t windowId, std::wstring_view layout)
    {
        auto remaining = _layoutStripHash(layout);
        std::unordered_set<int64_t> seen;

        seen.reserve(_attachedPanes.size() + 1);
        seen.insert(-1); // Always keep panes with id -1 (uninitialized)

        while (!remaining.empty())
        {
            const auto current = _layoutParseNextToken(remaining);
            if (current.type == TmuxLayoutType::Pane)
            {
                seen.insert(current.id);
            }
        }

        std::erase_if(_attachedPanes, [&](const auto& pair) {
            return pair.second.windowId == windowId && !seen.contains(pair.first);
        });
    }

    void TmuxControl::_handleResponse(std::wstring_view response)
    {
        // The first begin/end block we receive will come unprompted from tmux.
        if (_state == State::Init)
        {
            _handleAttach();
            return;
        }

        if (_commandQueue.empty())
        {
            // tmux should theoretically not send us any output blocks unprompted.
            assert(false);
            return;
        }

        const auto info = _commandQueue.front();
        _commandQueue.pop_front();

        switch (info.type)
        {
        case ResponseInfoType::Ignore:
            break;
        case ResponseInfoType::DiscoverNewWindow:
            _handleResponseDiscoverNewWindow(response);
            break;
        case ResponseInfoType::DiscoverWindows:
            _handleResponseDiscoverWindows(response);
            break;
        case ResponseInfoType::CapturePane:
            _handleResponseCapturePane(info, response);
            break;
        case ResponseInfoType::DiscoverPanes:
            _handleResponseDiscoverPanes(response);
            break;
        }
    }

    void TmuxControl::_sendSetOption(std::wstring_view option)
    {
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"set-option {}\n"), option));
    }

    // When we join a brand new session, tmux will output:
    //   %begin 1765124793 272 0
    //   %end 1765124793 272 0
    //   %window-add @0
    //   %sessions-changed
    //   %session-changed $0 0
    //   %window-renamed @0 tmux
    //   %output %0 ...
    // whereas if we join an existing session, we get:
    //   %begin 1765125530 285 0
    //   %end 1765125530 285 0
    //   %session-changed $0 0
    //
    // Because of this, we have to send a `list-windows` command ourselves.
    // We do this after the `session-changed` notification, because at that point we
    // received any potential `window-add` notifications that would indicate a new session.
    void TmuxControl::_sendDiscoverWindows(int64_t sessionId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-windows -t ${} -F '#{{session_id}} #{{window_id}} #{{window_width}} #{{window_height}} #{{history_limit}} #{{window_active}} #{{window_layout}} #{{window_name}}'\n"), sessionId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::DiscoverWindows,
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseDiscoverWindows(std::wstring_view response)
    {
        while (!response.empty())
        {
            auto line = split_line(response);
            const auto sessionId = tokenize_identifier(line);
            const auto windowId = tokenize_identifier(line);
            const auto windowWidth = tokenize_number(line);
            const auto windowHeight = tokenize_number(line);
            const auto historyLimit = tokenize_number(line);
            const auto windowActive = tokenize_number(line);
            const auto windowLayout = tokenize_field(line);
            const auto windowName = line;

            if (sessionId.type != IdentifierType::Session ||
                windowId.type != IdentifierType::Window ||
                !windowWidth ||
                !windowHeight ||
                !historyLimit ||
                !windowActive ||
                windowName.empty())
            {
                assert(false);
                continue;
            }

            if (_attachedWindows.contains(windowId.value))
            {
                print_debug(L"--> _handleResponseDiscoverWindows: skip {}\n", windowId.value);
                continue;
            }

            print_debug(L"--> _handleResponseDiscoverWindows: new window {}\n", windowId.value);

            auto remaining = _layoutStripHash(windowLayout);
            const auto firstPane = _layoutCreateRecursive(windowId.value, remaining, TmuxLayout{});
            _newTab(windowId.value, winrt::hstring{ windowName }, firstPane);

            // I'm not sure if I'm missing anything when I read the tmux spec,
            // but to me it seems like it's an inherently a racy protocol.
            // As a best-effort attempt we resize first (= potentially generates output, which we then ignore),
            // then we capture the panes' content (after which we stop ignoring output,
            // and finally we fix the current cursor position, and similar terminal state.
            _sendResizeWindow(windowId.value, _terminalWidth, _terminalHeight);
            for (auto& p : _attachedPanes)
            {
                if (p.second.windowId == windowId.value)
                {
                    // Discard any output we got/get until we captured the pane.
                    p.second.ignoreOutput = true;
                    p.second.outputBacklog.clear();

                    _sendCapturePane(p.second.paneId, (til::CoordType)*historyLimit);
                }
            }
            _sendDiscoverPanes(windowId.value);
        }

        _state = State::Attached;
    }

    std::shared_ptr<Pane> TmuxControl::_layoutCreateRecursive(int64_t windowId, std::wstring_view& remaining, TmuxLayout parent)
    {
        const auto direction = parent.type == TmuxLayoutType::PushVertical ? SplitDirection::Down : SplitDirection::Right;
        auto layoutSize = direction == SplitDirection::Right ? parent.width : parent.height;
        std::shared_ptr<Pane> firstPane;
        std::shared_ptr<Pane> lastPane;
        til::CoordType lastPaneSize = 0;

        while (!remaining.empty())
        {
            const auto current = _layoutParseNextToken(remaining);
            std::shared_ptr<Pane> pane;

            switch (current.type)
            {
            case TmuxLayoutType::Pane:
                pane = _newPane(windowId, current.id).second;
                break;
            case TmuxLayoutType::PushHorizontal:
            case TmuxLayoutType::PushVertical:
                print_debug(L"--> _handleResponseDiscoverWindows: recurse {}\n", current.type == TmuxLayoutType::PushHorizontal ? L"horizontal" : L"vertical");
                pane = _layoutCreateRecursive(windowId, remaining, current);
                break;
            case TmuxLayoutType::Pop:
                print_debug(L"--> _handleResponseDiscoverWindows: recurse pop\n");
                return firstPane;
            }

            if (!pane)
            {
                assert(false);
                continue;
            }

            if (!firstPane)
            {
                firstPane = pane;
            }
            if (lastPane)
            {
                const auto splitSize = 1.0f - ((float)lastPaneSize / (float)layoutSize);
                layoutSize -= lastPaneSize;

                print_debug(L"--> _handleResponseDiscoverWindows: new pane {} @ {:.1f}%\n", current.id, splitSize * 100);
                lastPane->AttachPane(pane, direction, splitSize);
            }
            else
            {
                print_debug(L"--> _handleResponseDiscoverWindows: new pane {}\n", current.id);
            }

            lastPane = std::move(pane);
            lastPaneSize = direction == SplitDirection::Right ? current.width : current.height;
            lastPaneSize += 1; // to account for tmux's separator line
        }

        return firstPane;
    }

    std::wstring_view TmuxControl::_layoutStripHash(std::wstring_view str)
    {
        const auto comma = str.find(L',');
        if (comma != std::wstring_view::npos)
        {
            return str.substr(comma + 1);
        }
        else
        {
            assert(false);
            return {};
        }
    }

    // Example layouts:
    // * single pane:
    //     cafd,120x29,0,0,0
    // * single horizontal split:
    //     813e,120x29,0,0{60x29,0,0,0,59x29,61,0,1}
    // * double horizontal split:
    //     04d9,120x29,0,0{60x29,0,0,0,29x29,61,0,1,29x29,91,0,2}
    // * double horizontal split + single vertical split in the middle pane:
    //     773d,120x29,0,0{60x29,0,0,0,29x29,61,0[29x14,61,0,1,29x14,61,15,3],29x29,91,0,2}
    TmuxControl::TmuxLayout TmuxControl::_layoutParseNextToken(std::wstring_view& remaining)
    {
        TmuxLayout layout{ .type = TmuxLayoutType::Pop };

        if (remaining.empty())
        {
            assert(false);
            return layout;
        }

        int64_t args[5];
        size_t arg_count = 0;
        wchar_t sep = L'\0';

        // Collect up to 5 arguments and the final separator
        //   120x29,0,0,2, --> 120, 29, 0, 0, 2  + ','
        //   120x29,0,0{   --> 120, 29, 0, 0     + '{'
        for (int i = 0; i < 5; ++i)
        {
            if (remaining.empty())
            {
                // Failed to collect enough args? Error.
                assert(false);
                return layout;
            }

            // If we're looking at a push/pop operation, break out. This is important
            // for the latter, because nested layouts may end in `]]]`, etc.
            sep = remaining[0];
            if (sep == L'[' || sep == L']' || sep == L'{' || sep == L'}')
            {
                remaining = remaining.substr(1);
                break;
            }

            // Skip 1 separator. Technically we should validate their correct position here, but meh.
            if (sep == L',' || sep == L'x')
            {
                remaining = remaining.substr(1);
                // We don't need to revalidate `remaining.empty()`,
                // because parse_signed will return nullopt for empty strings.
            }

            const auto end = std::min(remaining.size(), remaining.find_first_of(L",x[]{}"));
            const auto val = til::parse_signed<int64_t>(remaining.substr(0, end), 10);
            if (!val)
            {
                // Not an integer? Error.
                assert(false);
                return layout;
            }

            args[arg_count++] = *val;
            remaining = remaining.substr(end);
        }

        switch (sep)
        {
        case L'[':
        case L'{':
            if (arg_count != 4)
            {
                assert(false);
                return layout;
            }
            layout.type = sep == L'[' ? TmuxLayoutType::PushVertical : TmuxLayoutType::PushHorizontal;
            layout.width = (til::CoordType)args[0];
            layout.height = (til::CoordType)args[1];
            return layout;
        case L']':
        case L'}':
            if (arg_count != 0)
            {
                assert(false);
                return layout;
            }
            // layout.type is already set to Pop.
            return layout;
        default:
            if (arg_count != 5)
            {
                assert(false);
                return layout;
            }
            layout.type = TmuxLayoutType::Pane;
            layout.width = (til::CoordType)args[0];
            layout.height = (til::CoordType)args[1];
            layout.id = args[4];
            return layout;
        }
    }

    void TmuxControl::_sendDiscoverNewWindow(int64_t windowId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-panes -t @{} -F '#{{window_id}} #{{pane_id}} #{{window_name}}'\n"), windowId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::DiscoverNewWindow,
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseDiscoverNewWindow(std::wstring_view response)
    {
        print_debug(L"--> _handleResponseDiscoverNewWindow\n");

        const auto windowId = tokenize_identifier(response);
        const auto paneId = tokenize_identifier(response);
        const auto windowName = response;

        if (windowId.type == IdentifierType::Window && paneId.type == IdentifierType::Pane)
        {
            auto pane = _newPane(windowId.value, paneId.value).second;
            _newTab(windowId.value, winrt::hstring{ windowName }, std::move(pane));
        }
        else
        {
            assert(false);
        }
    }

    void TmuxControl::_sendCapturePane(int64_t paneId, til::CoordType history)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"capture-pane -epqCJN -S {} -t %{}\n"), -history, paneId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::CapturePane,
            .data = {
                .capturePane = {
                    .paneId = paneId,
                },
            },
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseCapturePane(const ResponseInfo& info, std::wstring_view response)
    {
        print_debug(L"--> _handleResponseCapturePane\n");

        const auto p = _attachedPanes.find(info.data.capturePane.paneId);
        if (p != _attachedPanes.end())
        {
            p->second.ignoreOutput = false;
            _deliverOutputToPane(info.data.capturePane.paneId, response);
        }
    }

    void TmuxControl::_sendDiscoverPanes(int64_t windowId)
    {
        // TODO: Here we would need to fetch much more than just the cursor position.
        const auto cmd = fmt::format(FMT_COMPILE(L"list-panes -t @{} -F '#{{pane_id}} #{{cursor_x}} #{{cursor_y}}'\n"), windowId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::DiscoverPanes,
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseDiscoverPanes(std::wstring_view response)
    {
        while (!response.empty())
        {
            auto line = split_line(response);
            const auto paneId = tokenize_identifier(line);
            const auto cursorX = tokenize_number(line);
            const auto cursorY = tokenize_number(line);

            if (paneId.type == IdentifierType::Pane && cursorX && cursorY)
            {
                const auto str = fmt::format(FMT_COMPILE(L"\033[{};{}H"), (til::CoordType)*cursorY + 1, (til::CoordType)*cursorX + 1);
                _deliverOutputToPane(paneId.value, str);
            }
            else
            {
                assert(false);
            }
        }
    }

    void TmuxControl::_sendNewWindow()
    {
        _sendIgnoreResponse(L"new-window\n");
    }

    void TmuxControl::_sendKillWindow(int64_t windowId)
    {
        // If we get a window-closed event, we call .Close() on the tab.
        // But that will raise a Closed event which will in turn call this function.
        // To avoid any loops, just check real quick if this window even exists anymore.
        if (_attachedWindows.erase(windowId) != 0)
        {
            std::erase_if(_attachedPanes, [windowId](const auto& pair) {
                return pair.second.windowId == windowId;
            });

            _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"kill-window -t @{}\n"), windowId));
        }
    }

    void TmuxControl::_sendKillPane(int64_t paneId)
    {
        // Same reasoning as in _sendKillWindow as to why we check `_attachedPanes`.
        if (const auto nh = _attachedPanes.extract(paneId))
        {
            const auto windowId = nh.mapped().windowId;

            // Check if there are more panes left in this window.
            // If so, we kill this pane only.
            for (const auto& p : _attachedPanes)
            {
                if (p.second.windowId == windowId)
                {
                    _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"kill-pane -t %{}\n"), paneId));
                    return;
                }
            }

            // Otherwise, we kill the whole window.
            _sendKillWindow(windowId);
        }
    }

    void TmuxControl::_sendSplitPane(std::shared_ptr<Pane> pane, SplitDirection direction)
    {
        if (_splittingPane.first != nullptr)
        {
            return;
        }

        if (!pane)
        {
            return;
        }

        int64_t paneId = -1;
        for (auto& p : _attachedPanes)
        {
            if (pane->GetTerminalControl() == p.second.control)
            {
                paneId = p.first;
            }
        }
        if (paneId == -1)
        {
            return;
        }

        _splittingPane = { pane, direction };

        const auto dir = direction == SplitDirection::Right ? L'h' : L'v';
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"split-window -t %{} -{}\n"), paneId, dir));
    }

    void TmuxControl::_sendSelectWindow(int64_t windowId)
    {
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"select-window -t @{}\n"), windowId));
    }

    void TmuxControl::_sendSelectPane(int64_t paneId)
    {
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"select-pane -t %{}\n"), paneId));
    }

    void TmuxControl::_sendResizeWindow(int64_t windowId, til::CoordType width, til::CoordType height)
    {
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"resize-window -t @{} -x {} -y {}\n"), windowId, width, height));
    }

    void TmuxControl::_sendResizePane(int64_t paneId, til::CoordType width, til::CoordType height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"resize-pane -t %{} -x {} -y {}\n"), paneId, width, height));
    }

    void TmuxControl::_sendSendKey(int64_t paneId, const std::wstring_view keys)
    {
        if (keys.empty())
        {
            return;
        }

        std::wstring buf;
        fmt::format_to(std::back_inserter(buf), FMT_COMPILE(L"send-key -t %{}"), paneId);
        for (auto& c : keys)
        {
            fmt::format_to(std::back_inserter(buf), FMT_COMPILE(L" {:#x}"), c);
        }
        buf.push_back(L'\n');
        _sendIgnoreResponse(buf);
    }

    void TmuxControl::_sendIgnoreResponse(wil::zwstring_view cmd)
    {
        print_debug(L">>> {}", cmd);

        if (!_control)
        {
            // This is unfortunately not uncommon right now due to the callback system.
            // Events may come in late during shutdown.
            print_debug(L"WARN: delayed send with uninitialized TmuxControl\n");
            return;
        }

        _control.RawWriteString(cmd);
        _commandQueue.push_back(ResponseInfo{
            .type = ResponseInfoType::Ignore,
        });
    }

    void TmuxControl::_sendWithResponseInfo(wil::zwstring_view cmd, ResponseInfo info)
    {
        print_debug(L">>> {}", cmd);

        if (!_control)
        {
            // This is unfortunately not uncommon right now due to the callback system.
            // Events may come in late during shutdown.
            print_debug(L"WARN: delayed send with uninitialized TmuxControl\n");
            return;
        }

        _control.RawWriteString(cmd);
        _commandQueue.push_back(info);
    }

    void TmuxControl::_deliverOutputToPane(int64_t paneId, const std::wstring_view text)
    {
        const auto search = _attachedPanes.find(paneId);
        if (search == _attachedPanes.end())
        {
            _attachedPanes.emplace_hint(
                search,
                paneId,
                AttachedPane{
                    .paneId = paneId,
                    .outputBacklog = std::wstring{ text },
                });
            return;
        }

        if (search->second.ignoreOutput)
        {
            return;
        }

        if (!search->second.initialized)
        {
            print_debug(L"--> outputBacklog {}\n", paneId);
            search->second.outputBacklog.append(text);
            return;
        }

        std::wstring out;
        auto it = text.begin();
        const auto end = text.end();

        while (it != end)
        {
            // Find start of any potential \xxx sequence
            const auto start = std::find(it, end, L'\\');

            // Copy any regular text
            out.append(it, start);
            it = start;
            if (it == end)
            {
                break;
            }

            // Process any \xxx sequences
            while (it != end && *it == L'\\')
            {
                ++it;

                wchar_t c = 0;
                for (int i = 0; i < 3 && it != end; ++i, ++it)
                {
                    if (*it < L'0' || *it > L'7')
                    {
                        c = L'?';
                        break;
                    }
                    c = c * 8 + (*it - L'0');
                }

                out.push_back(c);
            }
        }

        print_debug(L"--> _deliverOutputToPane {}\n", paneId);
        search->second.connection.WriteOutput(winrt_wstring_to_array_view(out));
    }

    winrt::com_ptr<Tab> TmuxControl::_getTab(int64_t windowId) const
    {
        const auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return nullptr;
        }
        return search->second;
    }

    void TmuxControl::_newTab(int64_t windowId, winrt::hstring name, std::shared_ptr<Pane> pane)
    {
        assert(!_attachedWindows.contains(windowId));
        auto tab = _page._GetTabImpl(_page._CreateNewTabFromPane(std::move(pane)));
        tab->SetTabText(name);
        tab->Closed([this, windowId](auto&&, auto&&) {
            _sendKillWindow(windowId);
        });
        _attachedWindows.emplace(windowId, std::move(tab));
    }

    std::pair<TmuxControl::AttachedPane&, std::shared_ptr<Pane>> TmuxControl::_newPane(int64_t windowId, int64_t paneId)
    {
        auto& p = _attachedPanes.try_emplace(paneId, AttachedPane{}).first->second;
        assert(p.windowId == -1);

        const auto controlSettings = Settings::TerminalSettings::CreateWithProfile(_page._settings, _profile);
        p.windowId = windowId;
        p.paneId = paneId;
        p.connection = TerminalConnection::TmuxConnection{};
        p.control = _page._CreateNewControlAndContent(controlSettings, p.connection);

        const auto pane = std::make_shared<Pane>(winrt::make<TerminalPaneContent>(_profile, _page._terminalSettingsCache, p.control));

        p.connection.TerminalInput([this, paneId](const winrt::array_view<const char16_t> keys) {
            _sendSendKey(paneId, winrt_array_to_wstring_view(keys));
        });

        p.control.Initialized([this, paneId](auto, auto) {
            const auto search = _attachedPanes.find(paneId);
            if (search == _attachedPanes.end())
            {
                return;
            }
            search->second.initialized = true;
            if (!search->second.outputBacklog.empty())
            {
                _deliverOutputToPane(paneId, std::move(search->second.outputBacklog));
                search->second.outputBacklog.clear();
            }
        });

        p.control.GotFocus([this, windowId, paneId](auto, auto) {
            if (_activePaneId == paneId)
            {
                return;
            }

            _activePaneId = paneId;
            _sendSelectPane(_activePaneId);

            if (_activeWindowId != windowId)
            {
                _activeWindowId = windowId;
                _sendSelectWindow(_activeWindowId);
            }
        });

        p.control.SizeChanged([this, paneId](auto, const Xaml::SizeChangedEventArgs& args) {
            if (_state != State::Attached)
            {
                return;
            }
            // Ignore the new created
            if (args.PreviousSize().Width == 0 || args.PreviousSize().Height == 0)
            {
                return;
            }

            const auto width = (til::CoordType)lrint((args.NewSize().Width - 2 * _thickness.Left) / _fontWidth);
            const auto height = (til::CoordType)lrint((args.NewSize().Height - 2 * _thickness.Top) / _fontHeight);
            _sendResizePane(paneId, width, height);
        });

        // Here's where we could use pane->Closed() to call _sendKillPane. Unfortunately, the entire Pane event handling
        // is very brittle. When you split a pane, most of its members (including the Closed event) stick to the new
        // parent (non-leaf) pane. You can't change that either, because the Closed() event of the root pane is used
        // to close the entire tab. There's no "pane split" event in order for the tab to know the root changed.
        // So, we hook into the connection's StateChanged event. It's only raised on connection.Close().
        // All of this would need a big, ugly refactor.
        p.connection.StateChanged([this, paneId](auto&&, auto&&) {
            _sendKillPane(paneId);
        });

        return { p, pane };
    }

    void TmuxControl::_openNewTerminalViaDropdown()
    {
        const auto window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const auto altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        if (altPressed)
        {
            // tmux panes don't share tab with other profile panes
            if (TabIsTmuxControl(_page._GetFocusedTabImpl()))
            {
                SplitPane(_page._GetFocusedTabImpl(), SplitDirection::Automatic);
            }
        }
        else
        {
            _sendNewWindow();
        }
    }
}
