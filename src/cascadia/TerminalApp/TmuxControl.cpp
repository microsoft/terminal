// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxControl.h"

#include <sstream>
#include <iostream>
#include <winrt/base.h>
#include <LibraryResources.h>

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

static const int PaneBorderSize = 2;
static const int StaticMenuCount = 4; // "Separator" "Settings" "Command Palette" "About"

namespace winrt::TerminalApp::implementation
{
    const std::wregex TmuxControl::REG_BEGIN{ L"^%begin (\\d+) (\\d+) (\\d+)$" };
    const std::wregex TmuxControl::REG_END{ L"^%end (\\d+) (\\d+) (\\d+)$" };
    const std::wregex TmuxControl::REG_ERROR{ L"^%error (\\d+) (\\d+) (\\d+)$" };

    const std::wregex TmuxControl::REG_CLIENT_SESSION_CHANGED{ L"^%client-session-changed (\\S+) \\$(\\d+) (\\S)+$" };
    const std::wregex TmuxControl::REG_CLIENT_DETACHED{ L"^%client-detached (\\S+)$" };
    const std::wregex TmuxControl::REG_CONFIG_ERROR{ L"^%config-error (\\S+)$" };
    const std::wregex TmuxControl::REG_CONTINUE{ L"^%continue %(\\d+)$" };
    const std::wregex TmuxControl::REG_DETACH{ L"^\033$" };
    const std::wregex TmuxControl::REG_EXIT{ L"^%exit$" };
    const std::wregex TmuxControl::REG_EXTENDED_OUTPUT{ L"^%extended-output %(\\d+) (\\S+)$" };
    const std::wregex TmuxControl::REG_LAYOUT_CHANGED{ L"^%layout-change @(\\d+) ([\\da-fA-F]{4}),(\\S+)( \\S+)*$" };
    const std::wregex TmuxControl::REG_MESSAGE{ L"^%message (\\S+)$" };
    const std::wregex TmuxControl::REG_OUTPUT{ L"^%output %(\\d+) (.+)$" };
    const std::wregex TmuxControl::REG_PANE_MODE_CHANGED{ L"^%pane-mode-changed %(\\d+)$" };
    const std::wregex TmuxControl::REG_PASTE_BUFFER_CHANGED{ L"^%paste-buffer-changed (\\S+)$" };
    const std::wregex TmuxControl::REG_PASTE_BUFFER_DELETED{ L"^%paste-buffer-deleted (\\S+)$" };
    const std::wregex TmuxControl::REG_PAUSE{ L"^%pause %(\\d+)$" };
    const std::wregex TmuxControl::REG_SESSION_CHANGED{ L"^%" L"session-changed \\$(\\d+) (\\S+)$" };
    const std::wregex TmuxControl::REG_SESSION_RENAMED{ L"^%" L"session-renamed (\\S+)$" };
    const std::wregex TmuxControl::REG_SESSION_WINDOW_CHANGED{ L"^%" L"session-window-changed @(\\d+) (\\d+)$" };
    const std::wregex TmuxControl::REG_SESSIONS_CHANGED{ L"^%" L"sessions-changed$" };
    const std::wregex TmuxControl::REG_SUBSCRIPTION_CHANGED{ L"^%" L"subscription-changed (\\S+)$" };
    const std::wregex TmuxControl::REG_UNLINKED_WINDOW_ADD{ L"^%unlinked-window-add @(\\d+)$" };
    const std::wregex TmuxControl::REG_UNLINKED_WINDOW_CLOSE{ L"^%unlinked-window-close @(\\d+)$" };
    const std::wregex TmuxControl::REG_UNLINKED_WINDOW_RENAMED{ L"^%unlinked-window-renamed @(\\d+)$" };
    const std::wregex TmuxControl::REG_WINDOW_ADD{ L"^%window-add @(\\d+)$" };
    const std::wregex TmuxControl::REG_WINDOW_CLOSE{ L"^%window-close @(\\d+)$" };
    const std::wregex TmuxControl::REG_WINDOW_PANE_CHANGED{ L"^%window-pane-changed @(\\d+) %(\\d+)$" };
    const std::wregex TmuxControl::REG_WINDOW_RENAMED{ L"^%window-renamed @(\\d+) (\\S+)$" };

    TmuxControl::TmuxControl(TerminalPage& page) :
        _page(page)
    {
        _dispatcherQueue = DispatcherQueue::GetForCurrentThread();

        _CreateNewTabMenu();
    }

    TmuxControl::StringHandler TmuxControl::TmuxControlHandlerProducer(const Control::TermControl control, const PrintHandler print)
    {
        std::lock_guard<std::mutex> guard(_inUseMutex);
        if (_inUse)
        {
            print(L"One session at same time");
            // Give any input to let tmux exit.
            _dispatcherQueue.TryEnqueue([control]() {
                control.RawWriteString(L"\n");
            });

            // Empty handler, do nothing, it will exit anyway.
            return [](const auto) {
                return true;
            };
        }

        _inUse = true;
        _control = control;
        _Print = print;

        _Print(L"Running the tmux control mode, press 'q' to detach:");

        return [this](const auto ch) {
            return _Advance(ch);
        };
    }

    bool TmuxControl::TabIsTmuxControl(const winrt::com_ptr<TerminalTab>& tab)
    {
        if (!tab)
        {
            return false;
        }

        for (auto& t : _attachedWindows)
        {
            if (t.second.TabViewIndex() == tab->TabViewIndex())
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

    void TmuxControl::SplitPane(const winrt::com_ptr<TerminalTab>& tab, SplitDirection direction)
    {
        const auto contentWidth = static_cast<float>(_page._tabContent.ActualWidth());
        const auto contentHeight = static_cast<float>(_page._tabContent.ActualHeight());
        const winrt::Windows::Foundation::Size availableSpace{ contentWidth, contentHeight };

        if (tab == nullptr)
        {
            return;
        }

        const auto realSplitType = tab.try_as<TerminalTab>()->PreCalculateCanSplit(direction, 0.5f, availableSpace);
        if (!realSplitType)
        {
            return;
        }

        switch(*realSplitType)
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

    void TmuxControl::_AttachSession()
    {
        _state = ATTACHING;

        _SetupProfile();

        // Intercept the control terminal's input, ignore all user input, except 'q' as detach command.
        _detachKeyDownRevoker = _control.KeyDown([this](auto, auto& e ) {
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

            _terminalWidth = (int)((x - _thickness.Left - _thickness.Right) / fontSize.Width);
            _terminalHeight = (int)((y - _thickness.Top - _thickness.Bottom) / fontSize.Height);
            _SetOption(std::format(L"default-size {}x{}", _terminalWidth, _terminalHeight));
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
        if (_state == INIT)
        {
            _inUse = false;
            return;
        }
        _state = INIT;
        _cmdQueue.clear();
        _dcsBuffer.clear();
        _cmdState = READY;

        std::vector<winrt::TerminalApp::TabBase> tabs;
        for (auto& w : _attachedWindows)
        {
            _page._RemoveTab(w.second);
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
        int i = 0;
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
        _thickness.Left = _thickness.Right = int((_fontWidth - 2 * PaneBorderSize) / 2);
        _thickness.Top = _thickness.Bottom = int((_fontHeight - 2 * PaneBorderSize) / 2);

        _terminalWidth = (int)((x - _thickness.Left - _thickness.Right) / fontSize.Width);
        _terminalHeight = (int)((y - _thickness.Top - _thickness.Bottom) / fontSize.Height);

        _profile.Padding(XamlThicknessToOptimalString(_thickness));
        _profile.ScrollState(winrt::Microsoft::Terminal::Control::ScrollbarState::Hidden);
        _profile.Icon(L"\uF714");
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

        _newTabMenu.Text(RS_(L"NewTmuxControlTab/Text"));
        Controls::ToolTipService::SetToolTip(_newTabMenu, box_value(textBlock));
        Controls::FontIcon newTabIcon{};
        newTabIcon.Glyph(L"\xF714");
        newTabIcon.FontFamily(Media::FontFamily{L"Segoe Fluent Icons,Segoe MDL2 Assets"});
        _newTabMenu.Icon(newTabIcon);

        _newTabMenu.Click([this](auto &&, auto&&) {
            _OpenNewTerminalViaDropdown();
        });
    }

    float TmuxControl::_ComputeSplitSize(int newSize, int originSize, SplitDirection direction) const
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

    TerminalApp::TerminalTab TmuxControl::_GetTab(int windowId) const
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

    void TmuxControl::_SendOutput(int paneId, const std::wstring& text)
    {
        auto search = _attachedPanes.find(paneId);

        // The pane is not ready it, put int backlog for now
        if (search == _attachedPanes.end())
        {
            _outputBacklog.insert_or_assign(paneId, text);
            return;
        }

        auto DecodeOutput = [](const std::wstring& in, std::wstring& out) {
            auto it = in.begin();
            while (it != in.end())
            {
                wchar_t c = *it;
                if (c == L'\\')
                {
                    ++it;
                    c = 0;
                    for (int i = 0; i < 3 && it != in.end(); ++i, ++it)
                    {
                        if (*it < L'0' || *it > L'7')
                        {
                            c = L'?';
                            break;
                        }
                        c = c * 8 + (*it - L'0');
                    }
                    out.push_back(c);
                    continue;
                }

                if (c == L'\n')
                {
                    out.push_back(L'\r');
                }

                out.push_back(c);
                ++it;
            }
        };

        auto& c = search->second.control;

        if (search->second.initialized) {
            std::wstring out = L"";
            DecodeOutput(text, out);
            c.SendOutput(out);
        }
        else
        {
            std::wstring res(text);
            c.Initialized([this, paneId, res](auto& /*i*/, auto& /*e*/) {
                _SendOutput(paneId, res);
            });
        }
    }

    void TmuxControl::_Output(int paneId, const std::wstring& result)
    {
        if (_state != ATTACHED)
        {
            return;
        }

        _SendOutput(paneId, result);
    }

    void TmuxControl::_CloseWindow(int windowId)
    {
        auto search = _attachedWindows.find(windowId);
        if (search == _attachedWindows.end())
        {
            return;
        }

        TerminalApp::TerminalTab t = search->second;
        _attachedWindows.erase(search);

        t.Shutdown();

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

        _page._RemoveTab(t);
    }

    void TmuxControl::_RenameWindow(int windowId, const std::wstring& name)
    {
        auto tab = _GetTab(windowId);
        if (tab == nullptr)
        {
            return;
        }

        tab.try_as<TerminalTab>()->SetTabText(winrt::hstring{ name });
    }

    void TmuxControl::_NewWindowFinalize(int windowId, int paneId, const std::wstring& windowName)
    {
        auto pane = _NewPane(windowId, paneId);
        auto tab = _page._CreateNewTabFromPane(pane);
        _attachedWindows.insert({windowId, tab});

        tab.try_as<TabBase>()->CloseRequested([this, windowId](auto &&, auto &&) {
            _KillWindow(windowId);
        });

        tab.try_as<TerminalTab>()->SetTabText(winrt::hstring{ windowName});

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

    void TmuxControl::_SplitPaneFinalize(int windowId, int newPaneId)
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

        auto activePane = tab.try_as<TerminalTab>()->GetActivePane();
        if (activePane.get() != _splittingPane.first.get())
        {
            return;
        }

        auto c = activePane->GetTerminalControl();

        int originSize;
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
        auto [origin, newGuy] = tab.try_as<TerminalTab>()->SplitPane(direction, splitSize, newPane);

        newGuy->GetTerminalControl().Focus(FocusState::Programmatic);
        _splittingPane.first = nullptr;
    }

    std::shared_ptr<Pane> TmuxControl::_NewPane(int windowId, int paneId)
    {
        auto connection = TerminalConnection::DummyConnection{};
        auto controlSettings = TerminalSettings::CreateWithProfile(_page._settings, _profile, *_page._bindings);
        const auto control = _page._CreateNewControlAndContent(controlSettings, connection);

        auto paneContent{ winrt::make<TerminalPaneContent> (_profile, _page._terminalSettingsCache, control) };
        auto pane = std::make_shared<Pane>(paneContent);

        control.Initialized([this, paneId](auto, auto) {
            auto search = _attachedPanes.find(paneId);
            if (search == _attachedPanes.end())
            {
                return;
            }
            search->second.initialized = true;
        });

        connection.TerminalInput([this, paneId](auto keys) {
            std::wstring out{ keys };
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
            if (_state != ATTACHED)
            {
                return;
            }
            // Ignore the new created
            if (args.PreviousSize().Width == 0 || args.PreviousSize().Height == 0)
            {
                return;
            }

            auto width = (int)((args.NewSize().Width - 2 * _thickness.Left) / _fontWidth);
            auto height = (int)((args.NewSize().Height - 2 * _thickness.Top) / _fontHeight);
            _ResizePane(paneId, width, height);
        });

        pane->Closed([this, paneId](auto&&, auto&&) {
            _KillPane(paneId);
        });

        _attachedPanes.insert({ paneId, {windowId, paneId, control} });

        return pane;
    }

    bool TmuxControl::_SyncPaneState(std::vector<TmuxPane> panes, int history)
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
            std::unordered_map<int, std::shared_ptr<Pane>> attachedPanes;
            for (auto& l : w.layout)
            {
                int rootSize;
                auto& panes = l.panes;
                auto& p = panes.at(0);
                switch (l.type)
                {
                    case SINGLE_PANE:
                        {
                            rootPane = _NewPane(w.windowId, p.id);
                            continue;
                        }
                    case SPLIT_HORIZONTAL:
                        direction = SplitDirection::Left;
                        rootSize = p.width;
                        break;
                    case SPLIT_VERTICAL:
                        direction = SplitDirection::Up;
                        rootSize = p.height;
                        break;
                }

                auto search = attachedPanes.find(p.id);
                std::shared_ptr<Pane> targetPane{ nullptr };
                int targetPaneId = p.id;
                if (search == attachedPanes.end())
                {
                    targetPane = _NewPane(w.windowId, p.id);
                    if (rootPane == nullptr) {
                        rootPane = targetPane;
                    }
                    attachedPanes.insert({p.id, targetPane});
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
                    attachedPanes.insert({p.id, pane});

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
                    attachedPanes.insert({targetPaneId, targetPane});
                    targetPane->Closed([this, targetPaneId](auto&&, auto&&) {
                        _KillPane(targetPaneId);
                    });
                }
            }
            auto tab = _page._CreateNewTabFromPane(rootPane);
            _attachedWindows.insert({w.windowId, tab});
            auto windowId = w.windowId;
            tab.try_as<TabBase>()->CloseRequested([this, windowId](auto &&, auto &&) {
                _KillWindow(windowId);
            });

            tab.try_as<TerminalTab>()->SetTabText(winrt::hstring{ w.name });
            _ListPanes(w.windowId, w.history);
        }
        return true;
    }

    std::vector<TmuxControl::TmuxWindowLayout> TmuxControl::_ParseTmuxWindowLayout(std::wstring& layout)
    {
        std::wregex RegPane { L"^,?(\\d+)x(\\d+),(\\d+),(\\d+),(\\d+)" };

        std::wregex RegSplitHorizontalPush { L"^,?(\\d+)x(\\d+),(\\d+),(\\d+)\\{" };
        std::wregex RegSplitVerticalPush { L"^,?(\\d+)x(\\d+),(\\d+),(\\d+)\\[" };
        std::wregex RegSplitPop { L"^[\\} | \\]]" };
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

            while (layout.length() > 0) {
                if (std::regex_search(layout, matches, RegSplitHorizontalPush)) {
                    TmuxPaneLayout p;
                    _ExtractPane(matches, p);
                    l.panes.push_back(p);
                    stack.push_back(l);

                    l.type = SPLIT_HORIZONTAL;
                    l.panes.clear();
                    l.panes.push_back(p);
                } else if (std::regex_search(layout, matches, RegSplitVerticalPush)) {
                    TmuxPaneLayout p;
                    _ExtractPane(matches, p);
                    l.panes.push_back(p);
                    stack.push_back(l);

                    // New one
                    l.type = SPLIT_VERTICAL;
                    l.panes.clear();
                    l.panes.push_back(p);
                } else if (std::regex_search(layout, matches, RegPane)) {
                    TmuxPaneLayout p;
                    _ExtractPane(matches, p);
                    l.panes.push_back(p);
                } else if (std::regex_search(layout, matches, RegSplitPop)) {
                    auto id = l.panes.back().id;
                    l.panes.pop_back();
                    l.panes.front().id = id;
                    result.insert(result.begin(), l);

                    l = stack.back();
                    l.panes.back().id = id;
                    stack.pop_back();
                } else {
                    assert(0);
                }
                parse_len = matches.length(0);
                layout = layout.substr(parse_len);
            }

            return result;
        };

        // Single pane mode
        std::wsmatch matches;
        if (std::regex_match(layout, matches, RegPane)) {
            TmuxPaneLayout p;
            _ExtractPane(matches, p);

            TmuxWindowLayout l;
            l.type = SINGLE_PANE;
            l.panes.push_back(p);

            result.push_back(l);
            return result;
        }

        // Nested mode
        _ParseNested(layout);

        return result;
    }

    void TmuxControl::_EventHandler(const Event& e)
    {
        switch(e.type)
        {
            case ATTACH:
                _AttachSession();
                break;
            case DETACH:
                _DetachSession();
                break;
            case LAYOUT_CHANGED:
                _DiscoverPanes(_sessionId, e.windowId, false);
                break;
            case OUTPUT:
                _Output(e.paneId, e.response);
                break;
            // Commands response
            case RESPONSE:
                _CommandHandler(e.response);
                break;
            case SESSION_CHANGED:
                _sessionId = e.sessionId;
                _SetOption(std::format(L"default-size {}x{}", _terminalWidth, _terminalHeight));
                _DiscoverWindows(_sessionId);
                break;
            case WINDOW_ADD:
                _DiscoverPanes(_sessionId, e.windowId, true);
                break;
            case WINDOW_CLOSE:
            case UNLINKED_WINDOW_CLOSE:
                _CloseWindow(e.windowId);
                break;
            case WINDOW_PANE_CHANGED:
                _SplitPaneFinalize(e.windowId, e.paneId);
                break;
            case WINDOW_RENAMED:
                _RenameWindow(e.windowId, e.response);
                break;

            default:
                break;
        }

        // We are done, give the command in the queue a chance to run
        _ScheduleCommand();
    }

    void TmuxControl::_Parse(const std::wstring& line)
    {
        std::wsmatch matches;

        // Tmux generic rules
        if (std::regex_match(line, REG_BEGIN))
        {
            _event.type = BEGIN;
        }
        else if (std::regex_match(line, REG_END))
        {
            if (_state == INIT)
            {
                _event.type = ATTACH;
            }
            else
            {
                _event.type = RESPONSE;
            }
        }
        else if (std::regex_match(line, REG_ERROR))
        {
            // Remove the extra '\n' we added
            _Print(std::wstring(_event.response.begin(), _event.response.end() - 1));
            _event.response.clear();
            _event.type = NOTHING;
        }

        // tmux specific rules
        else if (std::regex_match(line, REG_DETACH))
        {
            _event.type = DETACH;
        }
        else if (std::regex_match(line, matches, REG_LAYOUT_CHANGED))
        {
            _event.windowId = std::stoi(matches.str(1));
            _event.type = LAYOUT_CHANGED;
        }
        else if (std::regex_match(line, matches, REG_OUTPUT))
        {
            _event.paneId = std::stoi(matches.str(1));
            _event.response = matches.str(2);
            _event.type = OUTPUT;
        }
        else if (std::regex_match(line, matches, REG_SESSION_CHANGED))
        {
            _event.type = SESSION_CHANGED;
            _event.sessionId = std::stoi(matches.str(1));
        }
        else if (std::regex_match(line, matches, REG_WINDOW_ADD))
        {
            _event.windowId = std::stoi(matches.str(1));
            _event.type = WINDOW_ADD;
        }
        else if (std::regex_match(line, matches, REG_WINDOW_CLOSE))
        {
            _event.type = WINDOW_CLOSE;
            _event.windowId = std::stoi(matches.str(1));
        }
        else if (std::regex_match(line, matches, REG_WINDOW_PANE_CHANGED))
        {
            _event.type = WINDOW_PANE_CHANGED;
            _event.windowId = std::stoi(matches.str(1));
            _event.paneId = std::stoi(matches.str(2));
        }
        else if (std::regex_match(line, matches, REG_WINDOW_RENAMED))
        {
            _event.windowId = std::stoi(matches.str(1));
            _event.response = matches.str(2);
            _event.type = WINDOW_RENAMED;
        }
        else if (std::regex_match(line, matches, REG_UNLINKED_WINDOW_CLOSE))
        {
            _event.type = UNLINKED_WINDOW_CLOSE;
            _event.windowId = std::stoi(matches.str(1));
        }
        else
        {
            if (_event.type == BEGIN)
            {
                _event.response += line + L'\n';
            }
            else
            {
                // Other events that we don't care, do nothing
                _event.type = NOTHING;
            }
        }

        if (_event.type != BEGIN && _event.type != NOTHING)
        {
            auto& e = _event;
            _dispatcherQueue.TryEnqueue([this, e]() {
                _EventHandler(e);
            });
            _event.response.clear();
        }

        return;
    }

    // From tmux to controller through the dcs. parse it per line.
    bool TmuxControl::_Advance(wchar_t ch)
    {
        std::wstring buffer = L"";

        switch(ch)
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

    // Commands
    void TmuxControl::_AttachDone()
    {
        auto cmd = std::make_unique<AttachDone>();
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::AttachDone::GetCommand()
    {
        return std::wstring(std::format(L"list-session\n"));
    }

    bool TmuxControl::AttachDone::ResultHandler(const std::wstring& /*result*/, TmuxControl& tmux)
    {
        if (tmux._cmdQueue.size() > 1)
        {
            // Not done, requeue it, this is because capture may requeue in case the pane is not ready
            tmux._AttachDone();
        } else {
            tmux._state = ATTACHED;
        }

        return true;
    }

    void TmuxControl::_CapturePane(int paneId, int cursorX, int cursorY, int history)
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
        return std::wstring(std::format(L"capture-pane -p -t %{} -e -C -S {}\n", this->paneId, this->history * -1));
    }

    bool TmuxControl::CapturePane::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        // Tmux output has an extra newline
        std::wstring output = result;
        output.pop_back();
        // Put the cursor to right position
        output += std::format(L"\033[{};{}H", this->cursorY + 1, this->cursorX + 1);
        tmux._SendOutput(this->paneId, output);
        return true;
    }

    void TmuxControl::_DiscoverPanes(int sessionId, int windowId, bool newWindow)
    {
        if (_state != ATTACHED)
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
            return std::wstring(std::format(L"list-panes -s -F '"
                                            L"#{{pane_id}} #{{window_name}}"
                                            L"' -t ${}\n", this->sessionId));
        }
        else
        {
            return std::wstring(std::format(L"list-panes -F '"
                                            L"#{{pane_id}} #{{window_name}}"
                                            L"' -t @{}\n", this->windowId));
        }
    }

    bool TmuxControl::DiscoverPanes::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        std::wstring line;
        std::wregex REG_PANE{ L"^%(\\d+) (\\S+)$" };

        std::wstringstream in;
        in.str(result);

        std::set<int> panes;
        while (std::getline(in, line, L'\n'))
        {
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_PANE)) {
                continue;
            }
            int paneId = std::stoi(matches.str(1));
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
                auto activePane = tab.try_as<TerminalTab>()->GetActivePane();
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

    void TmuxControl::_DiscoverWindows(int sessionId)
    {
        auto cmd = std::make_unique<DiscoverWindows>();
        cmd->sessionId = sessionId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::DiscoverWindows::GetCommand()
    {
        return std::wstring(std::format(L"list-windows -F '"
                                        L"#{{window_id}}"
                                        L"' -t ${}\n", this->sessionId));
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

            if (!std::regex_match(line, matches, REG_WINDOW)) {
                continue;
            }
            int windowId = std::stoi(matches.str(1));
            tmux._ResizeWindow(windowId, tmux._terminalWidth, tmux._terminalHeight);
        }

        tmux._ListWindow(this->sessionId, -1);
        return true;
    }

    void TmuxControl::_KillPane(int paneId)
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
        return std::wstring(std::format(L"kill-pane -t %{}\n", this->paneId));
    }

    void TmuxControl::_KillWindow(int windowId)
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
        return std::wstring(std::format(L"kill-window -t @{}\n", this->windowId));
    }

    void TmuxControl::_ListPanes(int windowId, int history)
    {
        auto cmd = std::make_unique<ListPanes>();
        cmd->windowId = windowId;
        cmd->history = history;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ListPanes::GetCommand()
    {
        return std::wstring(std::format(L"list-panes -F '"
                                        L"#{{session_id}} #{{window_id}} #{{pane_id}} "
                                        L"#{{cursor_x}} #{{cursor_y}} "
                                        L"#{{pane_active}}"
                                        L"' -t @{}\n",
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

    void TmuxControl::_ListWindow(int sessionId, int windowId)
    {
        auto cmd = std::make_unique<ListWindow>();
        cmd->windowId = windowId;
        cmd->sessionId = sessionId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::ListWindow::GetCommand()
    {
        return std::wstring(std::format(L"list-windows -F '"
                                        L"#{{session_id}} #{{window_id}} "
                                        L"#{{window_width}} #{{window_height}} "
                                        L"#{{window_active}} "
                                        L"#{{window_layout}} "
                                        L"#{{window_name}} "
                                        L"#{{history_limit}}"
                                        L"' -t ${}\n", this->sessionId));
    }

    bool TmuxControl::ListWindow::ResultHandler(const std::wstring& result, TmuxControl& tmux)
    {
        std::wstring line;
        std::wregex REG_WINDOW{ L"^\\$(\\d+) @(\\d+) (\\d+) (\\d+) (\\d+) ([\\da-fA-F]{4}),(\\S+) (\\S+) (\\d+)$" };
        std::vector<TmuxWindow> windows;

        std::wstringstream in;
        in.str(result);

        while (std::getline(in, line, L'\n'))
        {
            TmuxWindow w;
            std::wsmatch matches;

            if (!std::regex_match(line, matches, REG_WINDOW)) {
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
            w.layout = tmux._ParseTmuxWindowLayout(layout);
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

    void TmuxControl::_ResizePane(int paneId, int width, int height)
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
        return std::wstring(std::format(L"resize-pane -x {} -y {} -t %{}\n", this->width, this->height, this->paneId));
    }

    void TmuxControl::_ResizeWindow(int windowId, int width, int height)
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
        return std::wstring(std::format(L"resize-window -x {} -y {} -t @{}\n", this->width, this->height, this->windowId));
    }

    void TmuxControl::_SelectPane(int paneId)
    {
        auto cmd = std::make_unique<SelectPane>();
        cmd->paneId = paneId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SelectPane::GetCommand()
    {
        return std::wstring(std::format(L"select-pane -t %{}\n", this->paneId));
    }

    void TmuxControl::_SelectWindow(int windowId)
    {
        auto cmd = std::make_unique<SelectWindow>();
        cmd->windowId = windowId;
        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SelectWindow::GetCommand()
    {
        return std::wstring(std::format(L"select-window -t @{}\n", this->windowId));
    }

    void TmuxControl::_SendKey(int paneId, const std::wstring keys)
    {
        auto cmd = std::make_unique<SendKey>();
        cmd->paneId = paneId;
        cmd->keys = keys;

        _SendCommand(std::move(cmd));
        _ScheduleCommand();
    }

    std::wstring TmuxControl::SendKey::GetCommand()
    {
        std::wstring out = L"";
        for (auto & c : this->keys)
        {
            out += std::format(L"{:#x} ", c);
        }

        return std::wstring(std::format(L"send-key -t %{} {}\n", this->paneId, out));
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
        return std::wstring(std::format(L"set-option {}\n", this->option));
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

        int paneId = -1;
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

        _splittingPane = {pane, direction};
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
            return std::wstring(std::format(L"split-window -h -t %{}\n", this->paneId));
        }
        else
        {
            return std::wstring(std::format(L"split-window -v -t %{}\n", this->paneId));
        }
    }

    // From controller to tmux
    void TmuxControl::_CommandHandler(const std::wstring& result)
    {
        if (_cmdState == WAITING && _cmdQueue.size() > 0)
        {
            auto cmd = _cmdQueue.front().get();
            cmd->ResultHandler(result, *this);
            _cmdQueue.pop_front();
            _cmdState = READY;
        }
    }

    void TmuxControl::_SendCommand(std::unique_ptr<TmuxControl::Command> cmd)
    {
        _cmdQueue.push_back(std::move(cmd));
    }

    void TmuxControl::_ScheduleCommand()
    {
        if (_cmdState != READY)
        {
            return;
        }

        if (_cmdQueue.size() > 0)
        {
            _cmdState = WAITING;

            auto cmd = _cmdQueue.front().get();
            auto cmdStr = cmd->GetCommand();
            _control.RawWriteString(cmdStr);
        }
    }
}
