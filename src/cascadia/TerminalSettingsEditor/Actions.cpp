// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Actions::Actions()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"Actions_AddNewTextBlock/Text"));
    }

    Automation::Peers::AutomationPeer Actions::OnCreateAutomationPeer()
    {
        _ViewModel.OnAutomationPeerAttached();
        return nullptr;
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ActionsViewModel>();

        // Subscribe to the view model's FocusContainer event.
        // Use the KeyBindingViewModel or index provided in the event to focus the corresponding container
        _ViewModel.FocusContainer([this](const auto& /*sender*/, const auto& args) {
            if (auto kbdVM{ args.try_as<KeyBindingViewModel>() })
            {
                if (const auto& container = KeyBindingsListView().ContainerFromItem(*kbdVM))
                {
                    container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                }
            }
            else if (const auto& index = args.try_as<uint32_t>())
            {
                if (const auto& container = KeyBindingsListView().ContainerFromIndex(*index))
                {
                    container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                }
            }
        });

        // Subscribe to the view model's UpdateBackground event.
        // The view model does not have access to the page resources, so it asks us
        // to update the key binding's container background
        _ViewModel.UpdateBackground([this](const auto& /*sender*/, const auto& args) {
            if (auto kbdVM{ args.try_as<KeyBindingViewModel>() })
            {
                if (kbdVM->IsInEditMode())
                {
                    const auto& containerBackground{ Resources().Lookup(box_value(L"ActionContainerBackgroundEditing")).as<Windows::UI::Xaml::Media::Brush>() };
                    kbdVM->ContainerBackground(containerBackground);
                }
                else
                {
                    const auto& containerBackground{ Resources().Lookup(box_value(L"ActionContainerBackground")).as<Windows::UI::Xaml::Media::Brush>() };
                    kbdVM->ContainerBackground(containerBackground);
                }
            }
        });

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("actions", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Actions::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
    {
        _ViewModel.AddNewKeybinding();
    }
}
