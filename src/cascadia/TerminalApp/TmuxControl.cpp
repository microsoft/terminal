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

static const til::CoordType PaneBorderSize = 2;
static const til::CoordType StaticMenuCount = 4; // "Separator" "Settings" "Command Palette" "About"

static std::wstring_view trim_newline(std::wstring_view str)
{
    auto end = str.end();
    if (end != str.begin() && end[-1] == L'\n')
    {
        --end;
        if (end != str.begin() && end[-1] == L'\r')
        {
            --end;
        }
    }
    return { str.begin(), end };
}

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

    remaining = til::safe_slice_abs(remaining, lf + 1, std::wstring_view::npos);
    return til::safe_slice_abs(remaining, 0, end);
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
    return til::parse_signed<int64_t>(tokenize_field(remaining), 10);
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

    const auto id = til::parse_signed<int64_t>(field.substr(1), 10);
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
        _page(page)
    {
        _dispatcherQueue = DispatcherQueue::GetForCurrentThread();

        auto newTabRun = Documents::Run();
        newTabRun.Text(RS_(L"NewTabRun/Text"));
        auto newPaneRun = Documents::Run();
        newPaneRun.Text(RS_(L"NewPaneRun/Text"));

        auto textBlock = Controls::TextBlock{};
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
            _OpenNewTerminalViaDropdown();
        });
    }

    bool TmuxControl::AcquireSingleUseLock(winrt::Microsoft::Terminal::Control::TermControl control) noexcept
    {
        if (_inUse.exchange(true, std::memory_order_relaxed))
        {
            return false;
        }

        _control = std::move(control);
        return true;
    }

    void TmuxControl::FeedInput(std::wstring_view str)
    {
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

            // Otherwise, parse the completed line.
            _parseLine(trim_newline({ _lineBuffer.begin(), _lineBuffer.end() }));
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

            _parseLine(til::safe_slice_abs(str, 0, end));

            str = til::safe_slice_abs(str, idx + 1, std::wstring_view::npos);
            idx = str.find(L'\n');
        }

        // If there's any leftover partial line, stash it for later.
        if (!str.empty())
        {
            _lineBuffer.insert(_lineBuffer.end(), str.begin(), str.end());
        }
    }

    bool TmuxControl::TabIsTmuxControl(const winrt::com_ptr<Tab>& tab)
    {
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

        if (_controlTab.TabViewIndex() == tab->TabViewIndex())
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

        return;
    }

    void TmuxControl::_parseLine(std::wstring_view remaining)
    {
        remaining = trim_newline(remaining);
        if (remaining.empty())
        {
            return;
        }

        const auto type = tokenize_field(remaining);

        // Are we inside a %begin ... %end block? Anything until %end or %error
        // is considered part of the output so this deserves special handling.
        if (_insideOutputBlock)
        {
            if (til::equals(type, L"%end"))
            {
                // The first begin/end block we receive will come unprompted from tmux.
                if (_state == State::INIT)
                {
                    _handleAttach();
                }
                else
                {
                    _handleResponse(_responseBuffer);
                }
                _responseBuffer.clear();
            }
            else if (til::equals(type, L"%error"))
            {
                // TODO: Where should we show this error?
                OutputDebugStringW(_responseBuffer.c_str());
                // In theory our commands should not result in errors.
                assert(false);
                _responseBuffer.clear();
            }
            else
            {
                if (!_responseBuffer.empty())
                {
                    _responseBuffer += L"\r\n";
                }
                _responseBuffer += remaining;
            }
        }
        // Otherwise, we check for the, presumably, most common output type first: %output.
        else if (til::equals(type, L"%output"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Pane)
            {
                _handleOutput(id.value, remaining);
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
                _sessionId = id.value;
                _sendSetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));
                _sendDiscoverWindows(_sessionId);
            }
        }
        else if (til::equals(type, L"%layout-change"))
        {
            // TODO: Do we even need to handle this?
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _sendDiscoverPanes(_sessionId);
            }
        }
        else if (til::equals(type, L"%window-add"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _sendDiscoverNewWindow(id.value);
            }
        }
        else if (til::equals(type, L"%window-close") || til::equals(type, L"%unlinked-window-close"))
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
                _handleWindowRenamed(id.value, remaining);
            }
        }
        else if (til::equals(type, L"\033"))
        {
            _handleDetach();
        }
    }

    void TmuxControl::_handleAttach()
    {
        _state = State::ATTACHING;

        {
            const auto settings{ CascadiaSettings::LoadDefaults() };
            _profile = settings.ProfileDefaults();
            if (const auto terminalTab{ _page._GetFocusedTabImpl() })
            {
                if (const auto pane{ terminalTab->GetActivePane() })
                {
                    _profile = settings.DuplicateProfile(pane->GetProfile());
                }
            }

            // Calculate our dimension
            auto fontSize = _control.CharacterDimensions();
            auto x = _page.ActualWidth();
            auto y = _page.ActualHeight();

            _fontWidth = fontSize.Width;
            _fontHeight = fontSize.Height;

            // Tmux use one character to draw separator line, so we have to make the padding
            // plus two borders equals one character's width or height
            // Same reason, we have to disable the scrollbar. Otherwise, the local panes size
            // will not match Tmux's.
            _thickness.Left = _thickness.Right = (til::CoordType)lrint((_fontWidth - 2 * PaneBorderSize) / 2);
            _thickness.Top = _thickness.Bottom = (til::CoordType)lrint((_fontHeight - 2 * PaneBorderSize) / 2);

            _terminalWidth = (til::CoordType)lrint((x - _thickness.Left - _thickness.Right) / fontSize.Width);
            _terminalHeight = (til::CoordType)lrint((y - _thickness.Top - _thickness.Bottom) / fontSize.Height);

            _profile.Padding(XamlThicknessToOptimalString(_thickness));
            _profile.ScrollState(winrt::Microsoft::Terminal::Control::ScrollbarState::Hidden);
            _profile.Icon(MediaResourceHelper::FromString(L"\uF714"));
            _profile.Name(L"TmuxTab");
        }

        // Intercept the control terminal's input, ignore all user input, except 'q' as detach command.
        _detachKeyDownRevoker = _control.KeyDown([this](auto, auto& e) {
            if (e.Key() == VirtualKey::Q)
            {
                _control.RawWriteString(L"detach\n");
            }
            e.Handled(true);
        });

        _windowSizeChangedRevoker = _page.SizeChanged([this](auto, auto) {
            auto fontSize = _control.CharacterDimensions();
            auto x = _page.ActualWidth();
            auto y = _page.ActualHeight();

            _terminalWidth = (til::CoordType)lrint((x - _thickness.Left - _thickness.Right) / fontSize.Width);
            _terminalHeight = (til::CoordType)lrint((y - _thickness.Top - _thickness.Bottom) / fontSize.Height);
            _sendSetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));
            for (auto& w : _attachedWindows)
            {
                _sendResizeWindow(w.first, _terminalWidth, _terminalHeight);
            }
        });

        // Dynamically insert the "Tmux Control Tab" menu item into flyout menu
        auto tabRow = _page.TabRow();
        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(tabRow);
        auto newTabButton = tabRowImpl->NewTabButton();

        auto menuCount = newTabButton.Flyout().try_as<Controls::MenuFlyout>().Items().Size();
        newTabButton.Flyout().try_as<Controls::MenuFlyout>().Items().InsertAt(menuCount - StaticMenuCount, _newTabMenu);

        // Register new tab button click handler for tmux control
        _newTabClickRevoker = newTabButton.Click([this](auto&&, auto&&) {
            if (TabIsTmuxControl(_page._GetFocusedTabImpl()))
            {
                _OpenNewTerminalViaDropdown();
            }
        });

        _controlTab = _page._GetFocusedTab();
    }

    void TmuxControl::_handleDetach()
    {
        if (_state == State::INIT)
        {
            _inUse = false;
            return;
        }

        _state = State::INIT;
        _commandQueue.clear();
        _lineBuffer.clear();

        for (auto& w : _attachedWindows)
        {
            _page._RemoveTab(*w.second);
        }
        _attachedPanes.clear();
        _attachedWindows.clear();

        // Revoke the event handlers
        _control.KeyDown(_detachKeyDownRevoker);
        _page.SizeChanged(_windowSizeChangedRevoker);

        // Remove the "Tmux Control Tab" menu item from flyout menu
        auto tabRow = _page.TabRow();
        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(tabRow);
        auto newTabButton = tabRowImpl->NewTabButton();
        uint32_t i = 0;
        for (const auto& m : newTabButton.Flyout().try_as<Controls::MenuFlyout>().Items())
        {
            if (m.try_as<Controls::MenuFlyoutItem>().Text() == RS_(L"NewTmuxControlTab/Text"))
            {
                newTabButton.Flyout().try_as<Controls::MenuFlyout>().Items().RemoveAt(i);
                break;
            }
            i++;
        }

        // Revoke the new tab button click handler
        newTabButton.Click(_newTabClickRevoker);

        _inUse = false;
        _control = nullptr;
        _controlTab = nullptr;
    }

    void TmuxControl::_handleOutput(int64_t paneId, const std::wstring_view result)
    {
        _deliverOutputToPane(paneId, result);
    }

    void TmuxControl::_handleWindowRenamed(int64_t windowId, const std::wstring_view name)
    {
        auto tab = _getTab(windowId);
        if (tab == nullptr)
        {
            return;
        }

        tab->SetTabText(winrt::hstring{ name });
    }

    void TmuxControl::_handleWindowClose(int64_t windowId)
    {
        const auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return;
        }

        const auto t = search->second;
        _attachedWindows.erase(search);

        t->Shutdown();

        // Remove all attached panes in this window
        for (auto p = _attachedPanes.begin(); p != _attachedPanes.end();)
        {
            if (p->second.windowId == windowId)
            {
                p = _attachedPanes.erase(p);
            }
            else
            {
                p++;
            }
        }

        _page._RemoveTab(*t);
    }

    void TmuxControl::_handleWindowPaneChanged(int64_t windowId, int64_t newPaneId)
    {
        // Only handle the split pane
        auto search = _attachedPanes.find(newPaneId);
        if (search != _attachedPanes.end())
        {
            return;
        }

        auto tab = _getTab(windowId);
        if (tab == nullptr)
        {
            return;
        }

        auto activePane = tab->GetActivePane();
        if (activePane.get() != _splittingPane.first.get())
        {
            return;
        }

        auto c = activePane->GetTerminalControl();

        til::CoordType originSize;
        auto direction = _splittingPane.second;
        if (direction == SplitDirection::Right)
        {
            originSize = c.ViewWidth();
        }
        else
        {
            originSize = c.ViewHeight();
        }

        auto newSize = originSize / 2;
        auto splitSize = _ComputeSplitSize(originSize - newSize, originSize, direction);
        auto newPane = _newPane(windowId, newPaneId);
        auto [origin, newGuy] = tab->SplitPane(direction, splitSize, newPane);

        newGuy->GetTerminalControl().Focus(FocusState::Programmatic);
        _splittingPane.first = nullptr;
    }

    void TmuxControl::_handleResponse(std::wstring_view response)
    {
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
        case ResponseInfoType::DiscoverPanes:
            _handleResponseDiscoverPanes(response);
            break;
        case ResponseInfoType::ListWindow:
            _handleResponseListWindow(response);
            break;
        case ResponseInfoType::ListPanes:
            _handleResponseListPanes(info, response);
            break;
        case ResponseInfoType::CapturePane:
            _handleResponseCapturePane(info, std::wstring{ response });
            break;
        }
    }

    void TmuxControl::_sendSetOption(std::wstring_view option)
    {
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"set-option {}\n"), option));
    }

    void TmuxControl::_sendDiscoverWindows(int64_t sessionId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-windows -F '#{{window_id}}' -t ${}\n"), sessionId);
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
            const auto id = tokenize_identifier(line);
            if (id.type == IdentifierType::Window)
            {
                _sendResizeWindow(id.value, _terminalWidth, _terminalHeight);
            }
        }

        // _sendDiscoverWindows() gets called with _sessionId as its parameter.
        // We should not be in a state where it's still unset.
        assert(_sessionId != -1);
        _sendListWindow(_sessionId);
    }

    void TmuxControl::_sendDiscoverNewWindow(int64_t windowId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-panes -F '#{{window_id}} #{{pane_id}} #{{window_name}}' -t @{}\n"), windowId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::DiscoverNewWindow,
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseDiscoverNewWindow(std::wstring_view response)
    {
        const auto windowId = tokenize_identifier(response);
        const auto paneId = tokenize_identifier(response);
        const auto windowName = response;

        if (windowId.type == IdentifierType::Pane && paneId.type == IdentifierType::Pane)
        {
            _newWindowFinalize(windowId.value, paneId.value, windowName);
        }
    }

    void TmuxControl::_sendDiscoverPanes(int64_t sessionId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-panes -s -F '#{{pane_id}}' -t ${}\n"), sessionId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::DiscoverPanes,
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseDiscoverPanes(std::wstring_view response)
    {
        std::unordered_set<int64_t> panes;
        while (!response.empty())
        {
            auto line = split_line(response);
            const auto id = tokenize_identifier(line);

            if (id.type == IdentifierType::Pane)
            {
                panes.insert(id.value);
            }
        }

        for (auto p = _attachedPanes.begin(); p != _attachedPanes.end();)
        {
            if (!panes.contains(p->first))
            {
                p->second.connection.Close();
                p = _attachedPanes.erase(p);
            }
            else
            {
                ++p;
            }
        }
    }

    void TmuxControl::_sendListWindow(int64_t sessionId)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-windows -F '#{{session_id}} #{{window_id}} #{{window_width}} #{{window_height}} #{{history_limit}} #{{window_active}} #{{window_layout}} #{{window_name}}' -t ${}\n"), sessionId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::ListWindow,
        };
        _sendWithResponseInfo(cmd, info);
    }

    std::vector<TmuxControl::TmuxWindowLayout> TmuxControl::_parseLayout(std::wstring_view remaining)
    {
        std::vector<TmuxControl::TmuxWindowLayout> result;
        std::vector<TmuxWindowLayout> stack;
        TmuxWindowLayout l;

        // caff,120x29,0,0,2
        // 5dc9,120x29,0,0{60x29,0,0,2,59x29,61,0[59x14,61,0,3,59x14,61,15,4]}

        // Strip off the layout hash
        const auto comma = remaining.find(L',');
        if (comma == std::wstring_view::npos)
        {
            return result;
        }
        remaining = remaining.substr(comma + 1);

        while (!remaining.empty())
        {
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
                    return result;
                }

                const auto end = std::min(remaining.size(), remaining.find_first_of(L",x[]{}"));
                const auto val = til::parse_signed<int64_t>(remaining.substr(0, end), 10);
                if (!val)
                {
                    // Not an integer? Error.
                    assert(false);
                    return result;
                }

                args[arg_count++] = *val;
                sep = (end < remaining.size()) ? remaining[end] : L'\0';

                remaining = remaining.substr(std::min(remaining.size(), end + 1));

                if (sep == L'[' || sep == L']' || sep == L'{' || sep == L'}')
                {
                    break;
                }
            }

            switch (sep)
            {
            case L'[':
            case L'{':
            {
                if (arg_count != 4)
                {
                    assert(false);
                    return result;
                }

                const auto p = TmuxPaneLayout{
                    .width = (til::CoordType)args[0],
                    .height = (til::CoordType)args[1],
                    .left = (til::CoordType)args[2],
                    .top = (til::CoordType)args[3],
                };
                l.panes.push_back(p);
                stack.push_back(l);

                // New one
                l.type = sep == L'[' ? TmuxLayoutType::SPLIT_VERTICAL : TmuxLayoutType::SPLIT_HORIZONTAL;
                l.panes.clear();
                l.panes.push_back(p);
                break;
            }
            case L']':
            case L'}':
            {
                auto id = l.panes.back().id;
                l.panes.pop_back();
                l.panes.front().id = id;
                result.insert(result.begin(), l);

                l = stack.back();
                l.panes.back().id = id;
                stack.pop_back();
                break;
            }
            default:
            {
                if (arg_count != 5)
                {
                    assert(false);
                    return result;
                }

                const auto p = TmuxPaneLayout{
                    .width = (til::CoordType)args[0],
                    .height = (til::CoordType)args[1],
                    .left = (til::CoordType)args[2],
                    .top = (til::CoordType)args[3],
                    .id = args[4],
                };
                l.panes.push_back(p);
                break;
            }
            }
        }

        return result;
    }

    void TmuxControl::_handleResponseListWindow(std::wstring_view response)
    {
        std::wstring line;
        std::wregex REG_WINDOW{ L"^\\$(\\d+) @(\\d+) (\\d+) (\\d+) (\\d+) ([\\da-fA-F]{4}),(\\S+) (\\S+) (\\d+)$" };
        std::vector<TmuxWindow> windows;

        // $1 @1 80  24 2000 1 5dc9,120x29,0,0{60x29,0,0,2,59x29,61,0[59x14,61,0,3,59x14,61,15,4]} fish
        // $2 @2 120 29 2000 1 caff,120x29,0,0,2 fish
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
                !windowActive)
            {
                continue;
            }

            windows.push_back(TmuxWindow{
                .sessionId = sessionId.value,
                .windowId = windowId.value,
                .width = (til::CoordType)*windowWidth,
                .height = (til::CoordType)*windowHeight,
                .history = (til::CoordType)*historyLimit,
                .active = (bool)*windowActive,
                .name = std::wstring{ windowName },
                .layout = _parseLayout(windowLayout),
            });
        }

        _state = State::ATTACHED;

        for (auto& w : windows)
        {
            auto direction = SplitDirection::Left;
            std::shared_ptr<Pane> rootPane{ nullptr };
            std::unordered_map<int64_t, std::shared_ptr<Pane>> attachedPanes;

            for (auto& l : w.layout)
            {
                til::CoordType rootSize;
                auto& panes = l.panes;
                auto& p = panes.at(0);
                switch (l.type)
                {
                case TmuxLayoutType::SINGLE_PANE:
                    rootPane = _newPane(w.windowId, p.id);
                    continue;
                case TmuxLayoutType::SPLIT_HORIZONTAL:
                    direction = SplitDirection::Left;
                    rootSize = p.width;
                    break;
                case TmuxLayoutType::SPLIT_VERTICAL:
                    direction = SplitDirection::Up;
                    rootSize = p.height;
                    break;
                }

                auto search = attachedPanes.find(p.id);
                std::shared_ptr<Pane> targetPane{ nullptr };
                auto targetPaneId = p.id;
                if (search == attachedPanes.end())
                {
                    targetPane = _newPane(w.windowId, p.id);
                    if (rootPane == nullptr)
                    {
                        rootPane = targetPane;
                    }
                    attachedPanes.insert({ p.id, targetPane });
                }
                else
                {
                    targetPane = search->second;
                }

                for (size_t i = 1; i < panes.size(); i++)
                {
                    // Create and attach
                    auto& p = panes.at(i);

                    auto pane = _newPane(w.windowId, p.id);
                    attachedPanes.insert({ p.id, pane });

                    float splitSize;
                    if (direction == SplitDirection::Left)
                    {
                        auto paneSize = panes.at(i).width;
                        splitSize = _ComputeSplitSize(paneSize, rootSize, direction);
                        rootSize -= (paneSize + 1);
                    }
                    else
                    {
                        auto paneSize = panes.at(i).height;
                        splitSize = _ComputeSplitSize(paneSize, rootSize, direction);
                        rootSize -= (paneSize + 1);
                    }
                    targetPane = targetPane->AttachPane(pane, direction, splitSize);
                    attachedPanes.erase(targetPaneId);
                    attachedPanes.insert({ targetPaneId, targetPane });
                    targetPane->Closed([this, targetPaneId](auto&&, auto&&) {
                        _sendKillPane(targetPaneId);
                    });
                }
            }
            auto tab = _page._GetTabImpl(_page._CreateNewTabFromPane(rootPane));
            _attachedWindows.emplace(w.windowId, tab);
            auto windowId = w.windowId;
            tab->CloseRequested([this, windowId](auto&&, auto&&) {
                _sendKillWindow(windowId);
            });

            tab->SetTabText(winrt::hstring{ w.name });
            _sendListPanes(w.windowId, w.history);
        }
    }

    void TmuxControl::_sendListPanes(int64_t windowId, til::CoordType history)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"list-panes -F '#{{pane_id}} #{{cursor_x}} #{{cursor_y}}' -t @{}\n"), windowId);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::ListPanes,
            .data = {
                .listPanes = {
                    .history = history,
                },
            },
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseListPanes(const ResponseInfo& info, std::wstring_view response)
    {
        std::vector<TmuxPane> panes;

        while (!response.empty())
        {
            auto line = split_line(response);
            const auto paneId = tokenize_identifier(line);
            const auto cursorX = tokenize_number(line);
            const auto cursorY = tokenize_number(line);

            if (paneId.type == IdentifierType::Pane && cursorX && cursorY)
            {
                panes.push_back(TmuxPane{
                    .paneId = paneId.value,
                    .cursorX = (til::CoordType)*cursorX,
                    .cursorY = (til::CoordType)*cursorY,
                });
            }
        }

        for (auto& p : panes)
        {
            auto search = _attachedPanes.find(p.paneId);
            if (search != _attachedPanes.end())
            {
                _sendCapturePane(p.paneId, p.cursorX, p.cursorY, info.data.listPanes.history);
            }
        }
    }

    void TmuxControl::_sendCapturePane(int64_t paneId, til::CoordType cursorX, til::CoordType cursorY, til::CoordType history)
    {
        const auto cmd = fmt::format(FMT_COMPILE(L"capture-pane -p -t %{} -e -C -S {}\n"), paneId, -history);
        const auto info = ResponseInfo{
            .type = ResponseInfoType::CapturePane,
            .data = {
                .capturePane = {
                    .paneId = paneId,
                    .cursorX = cursorX,
                    .cursorY = cursorY,
                },
            },
        };
        _sendWithResponseInfo(cmd, info);
    }

    void TmuxControl::_handleResponseCapturePane(const ResponseInfo& info, std::wstring response)
    {
        fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"\033[{};{}H"), info.data.capturePane.cursorY + 1, info.data.capturePane.cursorX + 1);
        _deliverOutputToPane(info.data.capturePane.paneId, response);
    }

    void TmuxControl::_sendNewWindow()
    {
        _sendIgnoreResponse(L"new-window\n");
    }

    void TmuxControl::_sendKillWindow(int64_t windowId)
    {
        auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return;
        }

        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"kill-window -t @{}\n"), windowId));
    }

    void TmuxControl::_sendKillPane(int64_t paneId)
    {
        auto search = _attachedPanes.find(paneId);
        if (search == _attachedPanes.end())
        {
            return;
        }

        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"kill-pane -t %{}\n"), paneId));
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
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"split-window -{} -t %{}\n"), dir, paneId));
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
        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"resize-window -x {} -y {} -t @{}\n"), width, height, windowId));
    }

    void TmuxControl::_sendResizePane(int64_t paneId, til::CoordType width, til::CoordType height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        _sendIgnoreResponse(fmt::format(FMT_COMPILE(L"resize-pane -x {} -y {} -t %{}\n"), width, height, paneId));
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

    float TmuxControl::_ComputeSplitSize(til::CoordType newSize, til::CoordType originSize, SplitDirection direction) const
    {
        float fontSize = _fontWidth;
        double margin1, margin2;
        if (direction == SplitDirection::Left || direction == SplitDirection::Right)
        {
            margin2 = _thickness.Left + _thickness.Right;
            margin1 = margin2 + PaneBorderSize;
        }
        else
        {
            fontSize = _fontHeight;
            margin2 = _thickness.Top + _thickness.Bottom;
            margin1 = margin2 + PaneBorderSize;
        }

        auto f = round(newSize * fontSize + margin1) / round(originSize * fontSize + margin2);

        return (float)(1.0f - f);
    }

    winrt::com_ptr<Tab> TmuxControl::_getTab(int64_t windowId) const
    {
        auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return nullptr;
        }

        return search->second;
    }

    void TmuxControl::_OpenNewTerminalViaDropdown()
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

    void TmuxControl::_deliverOutputToPane(int64_t paneId, const std::wstring_view text)
    {
        auto search = _attachedPanes.find(paneId);

        // The pane is not ready it, put in backlog for now
        if (search == _attachedPanes.end())
        {
            _outputBacklog.insert_or_assign(paneId, text);
            return;
        }

        if (!search->second.initialized)
        {
            search->second.control.Initialized([this, paneId, text](auto& /*i*/, auto& /*e*/) {
                _deliverOutputToPane(paneId, text);
            });
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

        search->second.connection.WriteOutput(winrt_wstring_to_array_view(out));
    }

    void TmuxControl::_newWindowFinalize(int64_t windowId, int64_t paneId, const std::wstring_view windowName)
    {
        auto pane = _newPane(windowId, paneId);
        auto tab = _page._GetTabImpl(_page._CreateNewTabFromPane(pane));
        _attachedWindows.emplace(windowId, tab);

        tab->CloseRequested([this, windowId](auto&&, auto&&) {
            _sendKillWindow(windowId);
        });

        tab->SetTabText(winrt::hstring{ windowName });

        // Check if we have output before we are ready
        auto search = _outputBacklog.find(paneId);
        if (search == _outputBacklog.end())
        {
            return;
        }

        auto& result = search->second;
        _deliverOutputToPane(paneId, result);
        _outputBacklog.erase(search);
    }

    std::shared_ptr<Pane> TmuxControl::_newPane(int64_t windowId, int64_t paneId)
    {
        auto connection = TerminalConnection::TmuxConnection{};
        auto controlSettings = Settings::TerminalSettings::CreateWithProfile(_page._settings, _profile);
        const auto control = _page._CreateNewControlAndContent(controlSettings, connection);

        auto paneContent{ winrt::make<TerminalPaneContent>(_profile, _page._terminalSettingsCache, control) };
        auto pane = std::make_shared<Pane>(paneContent);

        control.Initialized([this, paneId](auto, auto) {
            auto search = _attachedPanes.find(paneId);
            if (search == _attachedPanes.end())
            {
                return;
            }
            search->second.initialized = true;
        });

        connection.TerminalInput([this, paneId](const winrt::array_view<const char16_t> keys) {
            std::wstring out{ winrt_array_to_wstring_view(keys) };
            _sendSendKey(paneId, out);
        });

        control.GotFocus([this, windowId, paneId](auto, auto) {
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

        control.SizeChanged([this, paneId, control](auto, const Xaml::SizeChangedEventArgs& args) {
            if (_state != State::ATTACHED)
            {
                return;
            }
            // Ignore the new created
            if (args.PreviousSize().Width == 0 || args.PreviousSize().Height == 0)
            {
                return;
            }

            auto width = (til::CoordType)lrint((args.NewSize().Width - 2 * _thickness.Left) / _fontWidth);
            auto height = (til::CoordType)lrint((args.NewSize().Height - 2 * _thickness.Top) / _fontHeight);
            _sendResizePane(paneId, width, height);
        });

        pane->Closed([this, paneId](auto&&, auto&&) {
            _sendKillPane(paneId);
        });

        _attachedPanes.insert({ paneId, { windowId, paneId, control, connection } });

        return pane;
    }

    void TmuxControl::_sendIgnoreResponse(wil::zwstring_view cmd)
    {
        _control.RawWriteString(cmd);
        _commandQueue.push_back(ResponseInfo{
            .type = ResponseInfoType::Ignore,
        });
    }

    void TmuxControl::_sendWithResponseInfo(wil::zwstring_view cmd, ResponseInfo info)
    {
        _control.RawWriteString(cmd);
        _commandQueue.push_back(info);
    }
}
