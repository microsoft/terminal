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

static std::wstring_view tokenize_field(std::wstring_view& remaining)
{
    const auto beg = remaining.find_first_not_of(L' ');
    const auto end = remaining.find_first_of(L' ', beg);
    const auto field = til::safe_slice_abs(remaining, beg, end);
    remaining = til::safe_slice_abs(remaining, end, std::wstring_view::npos);
    return field;
}

static std::wstring_view tokenize_rest(std::wstring_view& remaining)
{
    const auto beg = remaining.find_first_not_of(L' ');
    const auto field = til::safe_slice_abs(remaining, beg, std::wstring_view::npos);
    remaining = {};
    return field;
}

enum class IdentifierType : wchar_t
{
    Invalid = 0,
    Session = '@',
    Window = '$',
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
    case L'@':
    case L'$':
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

        _CreateNewTabMenu();
    }

    bool TmuxControl::AcquireSingleUseLock() noexcept
    {
        return !_inUse.exchange(true, std::memory_order_relaxed);
    }

    void TmuxControl::FeedInput(std::wstring_view str) {
        for (auto&& ch : str)
        {
            _Advance(ch);
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
            _SplitPane(tab->GetActivePane(), SplitDirection::Right);
            break;
        case SplitDirection::Down:
            _SplitPane(tab->GetActivePane(), SplitDirection::Down);
            break;
        default:
            break;
        }

        return;
    }

    // From tmux to controller through the dcs. parse it per line.
    bool TmuxControl::_Advance(wchar_t ch)
    {
        std::wstring buffer;

        switch (ch)
        {
        case '\033':
            buffer.push_back(ch);
            break;
        case '\n':
            buffer = std::wstring(_dcsBuffer.begin(), _dcsBuffer.end());
            _dcsBuffer.clear();
            break;
        case '\r':
            break;
        default:
            _dcsBuffer.push_back(ch);
            break;
        }

        if (buffer.size() > 0)
        {
            _Parse(buffer);
        }

        return true;
    }

    void TmuxControl::_Parse(const std::wstring& line)
    {
        std::wstring_view remaining{ line };
        const auto type = tokenize_field(remaining);

        // Tmux generic rules
        if (til::equals(type, L"%begin"))
        {
            _event.type = EventType::BEGIN;
        }
        else if (til::equals(type, L"%end"))
        {
            if (_state == State::INIT)
            {
                _event.type = EventType::ATTACH;
            }
            else
            {
                _event.type = EventType::RESPONSE;
            }
        }
        else if (til::equals(type, L"%error"))
        {
            _event.response.pop_back(); // Remove the extra '\n' we added
            _Print(_event.response);
            _event.response.clear();
            _event.type = EventType::NOTHING;
        }
        // tmux specific rules
        else if (til::equals(type, L"\033"))
        {
            _event.type = EventType::DETACH;
        }
        else if (til::equals(type, L"%layout-change"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _event.windowId = id.value;
                _event.type = EventType::LAYOUT_CHANGED;
            }
        }
        else if (til::equals(type, L"%output"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Pane)
            {
                const auto payload = tokenize_rest(remaining);
                _event.paneId = id.value;
                _event.response = payload;
                _event.type = EventType::OUTPUT;
            }
        }
        else if (til::equals(type, L"%session-changed"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Session)
            {
                _event.type = EventType::SESSION_CHANGED;
                _event.sessionId = id.value;
            }
        }
        else if (til::equals(type, L"%window-add"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _event.windowId = id.value;
                _event.type = EventType::WINDOW_ADD;
            }
        }
        else if (til::equals(type, L"%window-close"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _event.windowId = id.value;
                _event.type = EventType::WINDOW_CLOSE;
            }
        }
        else if (til::equals(type, L"%window-pane-changed"))
        {
            const auto windowId = tokenize_identifier(remaining);
            const auto paneId = tokenize_identifier(remaining);

            if (windowId.type == IdentifierType::Window && paneId.type == IdentifierType::Pane)
            {
                _event.type = EventType::WINDOW_PANE_CHANGED;
                _event.windowId = windowId.value;
                _event.paneId = paneId.value;
            }
        }
        else if (til::equals(type, L"%window-renamed"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                const auto name = tokenize_rest(remaining);
                _event.windowId = id.value;
                _event.response = name;
                _event.type = EventType::WINDOW_RENAMED;
            }
        }
        else if (til::equals(type, L"%unlinked-window-close"))
        {
            const auto id = tokenize_identifier(remaining);
            if (id.type == IdentifierType::Window)
            {
                _event.type = EventType::UNLINKED_WINDOW_CLOSE;
                _event.windowId = id.value;
            }
        }
        else
        {
            if (_event.type == EventType::BEGIN)
            {
                _event.response += line + L'\n';
            }
            else
            {
                // Other events that we don't care, do nothing
                _event.type = EventType::NOTHING;
            }
        }

        if (_event.type != EventType::BEGIN && _event.type != EventType::NOTHING)
        {
            _dispatcherQueue.TryEnqueue([this]() {
                _EventHandler(_event);
            });
            _event.response.clear();
        }
    }

    void TmuxControl::_AttachSession()
    {
        _state = State::ATTACHING;

        _SetupProfile();

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
            _SetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));
            for (auto& w : _attachedWindows)
            {
                _ResizeWindow(w.first, _terminalWidth, _terminalHeight);
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

    void TmuxControl::_DetachSession()
    {
        if (_state == State::INIT)
        {
            _inUse = false;
            return;
        }

        _state = State::INIT;
        _cmdQueue.clear();
        _dcsBuffer.clear();
        _cmdState = CommandState::READY;

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
        _control = Control::TermControl(nullptr);
        _controlTab = nullptr;
    }

    // Tmux control has its own profile, we duplicate it from the control panel
    void TmuxControl::_SetupProfile()
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

    void TmuxControl::_CreateNewTabMenu()
    {
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

    winrt::com_ptr<Tab> TmuxControl::_GetTab(int64_t windowId) const
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
            _NewWindow();
        }
    }

    void TmuxControl::_SendOutput(int64_t paneId, const std::wstring& text)
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
                _SendOutput(paneId, text);
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
                wchar_t c = 0;
                int i = 0;

                while (i < 3 && it != end)
                {
                    if (*it < L'0' || *it > L'7')
                    {
                        c = L'?';
                        break;
                    }

                    c = c * 8 + (*it - L'0');
                    ++i;
                    ++it;
                }

                out.push_back(c);
            }
        }

        search->second.connection.WriteOutput(winrt_wstring_to_array_view(out));
    }

    void TmuxControl::_Output(int64_t paneId, const std::wstring& result)
    {
        if (_state != State::ATTACHED)
        {
            return;
        }

        _SendOutput(paneId, result);
    }

    void TmuxControl::_CloseWindow(int64_t windowId)
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

    void TmuxControl::_RenameWindow(int64_t windowId, const std::wstring& name)
    {
        auto tab = _GetTab(windowId);
        if (tab == nullptr)
        {
            return;
        }

        tab->SetTabText(winrt::hstring{ name });
    }

    void TmuxControl::_NewWindowFinalize(int64_t windowId, int64_t paneId, const std::wstring& windowName)
    {
        auto pane = _NewPane(windowId, paneId);
        auto tab = _page._GetTabImpl(_page._CreateNewTabFromPane(pane));
        _attachedWindows.emplace(windowId, tab);

        tab->CloseRequested([this, windowId](auto&&, auto&&) {
            _KillWindow(windowId);
        });

        tab->SetTabText(winrt::hstring{ windowName });

        // Check if we have output before we are ready
        auto search = _outputBacklog.find(paneId);
        if (search == _outputBacklog.end())
        {
            return;
        }

        auto& result = search->second;
        _SendOutput(paneId, result);
        _outputBacklog.erase(search);
    }

    void TmuxControl::_SplitPaneFinalize(int64_t windowId, int64_t newPaneId)
    {
        // Only handle the split pane
        auto search = _attachedPanes.find(newPaneId);
        if (search != _attachedPanes.end())
        {
            return;
        }

        auto tab = _GetTab(windowId);
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
        auto newPane = _NewPane(windowId, newPaneId);
        auto [origin, newGuy] = tab->SplitPane(direction, splitSize, newPane);

        newGuy->GetTerminalControl().Focus(FocusState::Programmatic);
        _splittingPane.first = nullptr;
    }

    std::shared_ptr<Pane> TmuxControl::_NewPane(int64_t windowId, int64_t paneId)
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
            _SendKey(paneId, out);
        });

        control.GotFocus([this, windowId, paneId](auto, auto) {
            if (_activePaneId == paneId)
            {
                return;
            }

            _activePaneId = paneId;
            _SelectPane(_activePaneId);

            if (_activeWindowId != windowId)
            {
                _activeWindowId = windowId;
                _SelectWindow(_activeWindowId);
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
            _ResizePane(paneId, width, height);
        });

        pane->Closed([this, paneId](auto&&, auto&&) {
            _KillPane(paneId);
        });

        _attachedPanes.insert({ paneId, { windowId, paneId, control, connection } });

        return pane;
    }

    bool TmuxControl::_SyncPaneState(std::vector<TmuxPane> panes, til::CoordType history)
    {
        for (auto& p : panes)
        {
            auto search = _attachedPanes.find(p.paneId);
            if (search == _attachedPanes.end())
            {
                continue;
            }

            _CapturePane(p.paneId, p.cursorX, p.cursorY, history);
        }

        return true;
    }

    bool TmuxControl::_SyncWindowState(std::vector<TmuxWindow> windows)
    {
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
                    rootPane = _NewPane(w.windowId, p.id);
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
                    targetPane = _NewPane(w.windowId, p.id);
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

                    auto pane = _NewPane(w.windowId, p.id);
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
                        _KillPane(targetPaneId);
                    });
                }
            }
            auto tab = _page._GetTabImpl(_page._CreateNewTabFromPane(rootPane));
            _attachedWindows.emplace(w.windowId, tab);
            auto windowId = w.windowId;
            tab->CloseRequested([this, windowId](auto&&, auto&&) {
                _KillWindow(windowId);
            });

            tab->SetTabText(winrt::hstring{ w.name });
            _ListPanes(w.windowId, w.history);
        }
        return true;
    }

    void TmuxControl::_EventHandler(const Event& e)
    {
        switch (e.type)
        {
        case EventType::ATTACH:
            _AttachSession();
            break;
        case EventType::DETACH:
            _DetachSession();
            break;
        case EventType::LAYOUT_CHANGED:
            _DiscoverPanes(_sessionId, e.windowId, false);
            break;
        case EventType::OUTPUT:
            _Output(e.paneId, e.response);
            break;
        // Commands response
        case EventType::RESPONSE:
            _CommandHandler(e.response);
            break;
        case EventType::SESSION_CHANGED:
            _sessionId = e.sessionId;
            _SetOption(fmt::format(FMT_COMPILE(L"default-size {}x{}"), _terminalWidth, _terminalHeight));
            _DiscoverWindows(_sessionId);
            break;
        case EventType::WINDOW_ADD:
            _DiscoverPanes(_sessionId, e.windowId, true);
            break;
        case EventType::WINDOW_CLOSE:
        case EventType::UNLINKED_WINDOW_CLOSE:
            _CloseWindow(e.windowId);
            break;
        case EventType::WINDOW_PANE_CHANGED:
            _SplitPaneFinalize(e.windowId, e.paneId);
            break;
        case EventType::WINDOW_RENAMED:
            _RenameWindow(e.windowId, e.response);
            break;
        default:
            break;
        }

        // We are done, give the command in the queue a chance to run
        _ScheduleCommand();
    }

    // Commands
    void TmuxControl::_AttachDone()
    {
        auto cmd = std::make_unique<AttachDone>();
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::AttachDone::GetCommand()
    {
        return std::wstring(L"list-session\n");
    }

    bool TmuxControl::AttachDone::ResultHandler(const std::wstring& /*result*/, TmuxControl& tmux)
    {
        if (tmux._cmdQueue.size() > 1)
        {
            // Not done, requeue it, this is because capture may requeue in case the pane is not ready
            tmux._AttachDone();
        }
        else
        {
            tmux._state = State::ATTACHED;
        }

        return true;
    }

    void TmuxControl::_CapturePane(int64_t paneId, til::CoordType cursorX, til::CoordType cursorY, til::CoordType history)
    {
        auto cmd = std::make_unique<CapturePane>();
        cmd->paneId = paneId;
        cmd->cursorX = cursorX;
        cmd->cursorY = cursorY;
        cmd->history = history;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::CapturePane::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"capture-pane -p -t %{} -e -C -S {}\n"), this->paneId, this->history * -1));
    }

    bool TmuxControl::CapturePane::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        // Tmux output has an extra newline
        std::wstring output = result;
        output.pop_back();
        // Put the cursor to right position
        output += fmt::format(FMT_COMPILE(L"\033[{};{}H"), this->cursorY + 1, this->cursorX + 1);
        tmux._SendOutput(this->paneId, output);
        return true;
    }

    void TmuxControl::_DiscoverPanes(int64_t sessionId, int64_t windowId, bool newWindow)
    {
        if (_state != State::ATTACHED)
        {
            return;
        }
        auto cmd = std::make_unique<DiscoverPanes>();
        cmd->sessionId = sessionId;
        cmd->windowId = windowId;
        cmd->newWindow = newWindow;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::DiscoverPanes::GetCommand()
    {
        if (!this->newWindow)
        {
            return std::wstring(fmt::format(FMT_COMPILE(L"list-panes -s -F '"
                                                        L"#{{pane_id}} #{{window_name}}"
                                                        L"' -t ${}\n"),
                                            this->sessionId));
        }
        else
        {
            return std::wstring(fmt::format(FMT_COMPILE(L"list-panes -F '"
                                                        L"#{{pane_id}} #{{window_name}}"
                                                        L"' -t @{}\n"),
                                            this->windowId));
        }
    }

    bool TmuxControl::DiscoverPanes::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        std::wstring line;
        std::wregex REG_PANE{ L"^%(\\d+) (\\S+)$" };

        std::wstringstream in;
        in.str(result);

        std::set<int64_t> panes;
        while (std::getline(in, line, L'\n'))
        {
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_PANE))
            {
                continue;
            }
            int64_t paneId = std::stoi(matches.str(1));
            std::wstring windowName = matches.str(2);
            // New window case, just one pane
            if (this->newWindow)
            {
                tmux._NewWindowFinalize(this->windowId, paneId, windowName);
                return true;
            }
            panes.insert(paneId);
        }

        // For pane exit case
        for (auto p = tmux._attachedPanes.begin(); p != tmux._attachedPanes.end();)
        {
            if (!panes.contains(p->first))
            {
                p = tmux._attachedPanes.erase(p);
                auto tab = tmux._GetTab(this->windowId);
                if (tab == nullptr)
                {
                    return true;
                }
                auto activePane = tab->GetActivePane();
                activePane->Close();
                return true;
            }
            else
            {
                p++;
            }
        }

        return true;
    }

    void TmuxControl::_DiscoverWindows(int64_t sessionId)
    {
        auto cmd = std::make_unique<DiscoverWindows>();
        cmd->sessionId = sessionId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::DiscoverWindows::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"list-windows -F '"
                                                    L"#{{window_id}}"
                                                    L"' -t ${}\n"),
                                        this->sessionId));
    }

    bool TmuxControl::DiscoverWindows::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        std::wstring line;
        std::wregex REG_WINDOW{ L"^@(\\d+)$" };

        std::wstringstream in;
        in.str(result);

        while (std::getline(in, line, L'\n'))
        {
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_WINDOW))
            {
                continue;
            }
            int64_t windowId = std::stoi(matches.str(1));
            tmux._ResizeWindow(windowId, tmux._terminalWidth, tmux._terminalHeight);
        }

        tmux._ListWindow(this->sessionId, -1);
        return true;
    }

    void TmuxControl::_KillPane(int64_t paneId)
    {
        auto search = _attachedPanes.find(paneId);
        if (search == _attachedPanes.end())
        {
            return;
        }

        auto cmd = std::make_unique<KillPane>();
        cmd->paneId = paneId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::KillPane::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"kill-pane -t %{}\n"), this->paneId));
    }

    void TmuxControl::_KillWindow(int64_t windowId)
    {
        auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return;
        }

        auto cmd = std::make_unique<KillWindow>();
        cmd->windowId = windowId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::KillWindow::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"kill-window -t @{}\n"), this->windowId));
    }

    void TmuxControl::_ListPanes(int64_t windowId, til::CoordType history)
    {
        auto cmd = std::make_unique<ListPanes>();
        cmd->windowId = windowId;
        cmd->history = history;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ListPanes::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"list-panes -F '"
                                                    L"#{{session_id}} #{{window_id}} #{{pane_id}} "
                                                    L"#{{cursor_x}} #{{cursor_y}} "
                                                    L"#{{pane_active}}"
                                                    L"' -t @{}\n"),
                                        this->windowId));
    }

    bool TmuxControl::ListPanes::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        std::wstring line;
        std::wregex REG_PANE{ L"^\\$(\\d+) @(\\d+) %(\\d+) (\\d+) (\\d+) (\\d+)$" };
        std::vector<TmuxPane> panes;

        std::wstringstream in;
        in.str(result);

        while (std::getline(in, line, L'\n'))
        {
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_PANE))
            {
                continue;
            }

            TmuxPane p = {
                .sessionId = std::stoi(matches.str(1)),
                .windowId = std::stoi(matches.str(2)),
                .paneId = std::stoi(matches.str(3)),
                .cursorX = std::stoi(matches.str(4)),
                .cursorY = std::stoi(matches.str(5)),
                .active = (std::stoi(matches.str(6)) == 1)
            };

            panes.push_back(p);
        }

        tmux._SyncPaneState(panes, this->history);
        return true;
    }

    void TmuxControl::_ListWindow(int64_t sessionId, int64_t windowId)
    {
        auto cmd = std::make_unique<ListWindow>();
        cmd->windowId = windowId;
        cmd->sessionId = sessionId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ListWindow::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"list-windows -F '"
                                                    L"#{{session_id}} #{{window_id}} "
                                                    L"#{{window_width}} #{{window_height}} "
                                                    L"#{{window_active}} "
                                                    L"#{{window_layout}} "
                                                    L"#{{window_name}} "
                                                    L"#{{history_limit}}"
                                                    L"' -t ${}\n"),
                                        this->sessionId));
    }

    bool TmuxControl::ListWindow::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        const auto _ParseTmuxWindowLayout = [](std::wstring& layout) -> std::vector<TmuxControl::TmuxWindowLayout> {
            std::wregex RegPane{ L"^,?(\\d+)x(\\d+),(\\d+),(\\d+),(\\d+)" };

            std::wregex RegSplitHorizontalPush{ L"^,?(\\d+)x(\\d+),(\\d+),(\\d+)\\{" };
            std::wregex RegSplitVerticalPush{ L"^,?(\\d+)x(\\d+),(\\d+),(\\d+)\\[" };
            std::wregex RegSplitPop{ L"^[\\} | \\]]" };
            std::vector<TmuxControl::TmuxWindowLayout> result;

            auto _ExtractPane = [&](std::wsmatch& matches, TmuxPaneLayout& p) {
                p.width = std::stoi(matches.str(1));
                p.height = std::stoi(matches.str(2));
                p.left = std::stoi(matches.str(3));
                p.top = std::stoi(matches.str(4));
                if (matches.size() > 5)
                {
                    p.id = std::stoi(matches.str(5));
                }
            };

            auto _ParseNested = [&](std::wstring) {
                std::wsmatch matches;
                size_t parse_len = 0;
                TmuxWindowLayout l;

                std::vector<TmuxWindowLayout> stack;

                while (layout.length() > 0)
                {
                    if (std::regex_search(layout, matches, RegSplitHorizontalPush))
                    {
                        TmuxPaneLayout p;
                        _ExtractPane(matches, p);
                        l.panes.push_back(p);
                        stack.push_back(l);

                        l.type = TmuxLayoutType::SPLIT_HORIZONTAL;
                        l.panes.clear();
                        l.panes.push_back(p);
                    }
                    else if (std::regex_search(layout, matches, RegSplitVerticalPush))
                    {
                        TmuxPaneLayout p;
                        _ExtractPane(matches, p);
                        l.panes.push_back(p);
                        stack.push_back(l);

                        // New one
                        l.type = TmuxLayoutType::SPLIT_VERTICAL;
                        l.panes.clear();
                        l.panes.push_back(p);
                    }
                    else if (std::regex_search(layout, matches, RegPane))
                    {
                        TmuxPaneLayout p;
                        _ExtractPane(matches, p);
                        l.panes.push_back(p);
                    }
                    else if (std::regex_search(layout, matches, RegSplitPop))
                    {
                        auto id = l.panes.back().id;
                        l.panes.pop_back();
                        l.panes.front().id = id;
                        result.insert(result.begin(), l);

                        l = stack.back();
                        l.panes.back().id = id;
                        stack.pop_back();
                    }
                    else
                    {
                        assert(0);
                    }
                    parse_len = matches.length(0);
                    layout = layout.substr(parse_len);
                }

                return result;
            };

            // Single pane mode
            std::wsmatch matches;
            if (std::regex_match(layout, matches, RegPane))
            {
                TmuxPaneLayout p;
                _ExtractPane(matches, p);

                TmuxWindowLayout l;
                l.type = TmuxLayoutType::SINGLE_PANE;
                l.panes.push_back(p);

                result.push_back(l);
                return result;
            }

            // Nested mode
            _ParseNested(layout);

            return result;
        };

        std::wstring line;
        std::wregex REG_WINDOW{ L"^\\$(\\d+) @(\\d+) (\\d+) (\\d+) (\\d+) ([\\da-fA-F]{4}),(\\S+) (\\S+) (\\d+)$" };
        std::vector<TmuxWindow> windows;

        std::wstringstream in;
        in.str(result);

        while (std::getline(in, line, L'\n'))
        {
            TmuxWindow w;
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_WINDOW))
            {
                continue;
            }
            w.sessionId = std::stoi(matches.str(1));
            w.windowId = std::stoi(matches.str(2));
            w.width = std::stoi(matches.str(3));
            w.height = std::stoi(matches.str(4));
            w.active = (std::stoi(matches.str(5)) == 1);
            w.layoutChecksum = matches.str(6);
            w.name = matches.str(8);
            w.history = std::stoi(matches.str(9));
            std::wstring layout(matches.str(7));
            w.layout = _ParseTmuxWindowLayout(layout);
            windows.push_back(w);
        }

        tmux._SyncWindowState(windows);
        tmux._AttachDone();
        return true;
    }

    void TmuxControl::_NewWindow()
    {
        auto cmd = std::make_unique<NewWindow>();
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::NewWindow::GetCommand()
    {
        return std::wstring(L"new-window\n");
    }

    void TmuxControl::_ResizePane(int64_t paneId, til::CoordType width, til::CoordType height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }
        auto cmd = std::make_unique<ResizePane>();
        cmd->paneId = paneId;
        cmd->width = width;
        cmd->height = height;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ResizePane::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"resize-pane -x {} -y {} -t %{}\n"), this->width, this->height, this->paneId));
    }

    void TmuxControl::_ResizeWindow(int64_t windowId, til::CoordType width, til::CoordType height)
    {
        auto cmd = std::make_unique<ResizeWindow>();
        cmd->windowId = windowId;
        cmd->width = width;
        cmd->height = height;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ResizeWindow::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"resize-window -x {} -y {} -t @{}\n"), this->width, this->height, this->windowId));
    }

    void TmuxControl::_SelectPane(int64_t paneId)
    {
        auto cmd = std::make_unique<SelectPane>();
        cmd->paneId = paneId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SelectPane::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"select-pane -t %{}\n"), this->paneId));
    }

    void TmuxControl::_SelectWindow(int64_t windowId)
    {
        auto cmd = std::make_unique<SelectWindow>();
        cmd->windowId = windowId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SelectWindow::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"select-window -t @{}\n"), this->windowId));
    }

    void TmuxControl::_SendKey(int64_t paneId, const std::wstring keys)
    {
        auto cmd = std::make_unique<SendKey>();
        cmd->paneId = paneId;
        cmd->keys = keys;

        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SendKey::GetCommand()
    {
        std::wstring out;
        for (auto& c : this->keys)
        {
            out += fmt::format(FMT_COMPILE(L"{:#x} "), c);
        }

        return std::wstring(fmt::format(FMT_COMPILE(L"send-key -t %{} {}\n"), this->paneId, out));
    }

    void TmuxControl::_SetOption(const std::wstring& option)
    {
        auto cmd = std::make_unique<SetOption>();
        cmd->option = option;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SetOption::GetCommand()
    {
        return std::wstring(fmt::format(FMT_COMPILE(L"set-option {}\n"), this->option));
    }

    void TmuxControl::_SplitPane(std::shared_ptr<Pane> pane, SplitDirection direction)
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
        auto cmd = std::make_unique<struct SplitPane>();
        cmd->direction = direction;
        cmd->paneId = paneId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SplitPane::GetCommand()
    {
        if (this->direction == SplitDirection::Right)
        {
            return std::wstring(fmt::format(FMT_COMPILE(L"split-window -h -t %{}\n"), this->paneId));
        }
        else
        {
            return std::wstring(fmt::format(FMT_COMPILE(L"split-window -v -t %{}\n"), this->paneId));
        }
    }

    // From controller to tmux
    void TmuxControl::_CommandHandler(const std::wstring& result)
    {
        if (_cmdState == CommandState::WAITING && _cmdQueue.size() > 0)
        {
            auto cmd = _cmdQueue.front().get();
            cmd->ResultHandler(result, *this);
            _cmdQueue.pop_front();
            _cmdState = CommandState::READY;
        }
    }

    void TmuxControl::_SendCommand(std::unique_ptr<TmuxControl::Command> cmd)
    {
        _cmdQueue.push_back(std::move(cmd));
    }

    void TmuxControl::_ScheduleCommand()
    {
        if (_cmdState != CommandState::READY)
        {
            return;
        }

        if (_cmdQueue.size() > 0)
        {
            _cmdState = CommandState::WAITING;

            auto cmd = _cmdQueue.front().get();
            auto cmdStr = cmd->GetCommand();
            _control.RawWriteString(cmdStr);
        }
    }
}
