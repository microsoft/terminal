// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "ActionsPageNavigationState.g.cpp"
#include "KeyBindingContainer.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Actions::Actions()
    {
        InitializeComponent();
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ActionsPageNavigationState>();

        std::vector<Command> keyBindingList;
        for (const auto& [_, command] : _State.Settings().GlobalSettings().ActionMap().NameMap())
        {
            // Filter out nested commands, and commands that aren't bound to a
            // key. This page is currently just for displaying the actions that
            // _are_ bound to keys.
            if (command.HasNestedCommands() || !command.Keys())
            {
                continue;
            }
            keyBindingList.push_back(command);
        }
        std::sort(begin(keyBindingList), end(keyBindingList), CommandComparator{});

        for (const auto& cmd : keyBindingList)
        {
            //const auto& container{ make<KeyBindingContainer>(cmd) };
            const auto& container{ make<CommandViewModel>(cmd) };
            KeyBindingListView().Items().Append(container);
        }
    }
}
