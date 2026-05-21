// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Actions::Actions()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"Actions_AddNewTextBlock/Text"));
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::ActionsViewModel>();
        _ViewModel.ReSortCommandList();
        auto vmImpl = get_self<ActionsViewModel>(_ViewModel);
        vmImpl->MarkAsVisited();
        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            AddNewButton().Focus(FocusState::Programmatic);
        });
        BringIntoViewWhenLoaded(args.ElementToFocus());

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("actions", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    // Builds the "additional key chords" flyout lazily on the first click of a
    // row's "+N" button, then caches it on the button so subsequent clicks
    // just re-show it.
    void Actions::AdditionalKeyChordsButton_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto button = sender.try_as<Button>();
        if (!button)
        {
            return;
        }

        // Retrieve cached flyout, if possible
        if (const auto existing = button.Flyout())
        {
            existing.ShowAt(button);
            return;
        }

        const auto cmdVM = button.DataContext().try_as<Editor::CommandViewModel>();
        if (!cmdVM)
        {
            return;
        }

        Flyout flyout;
        flyout.Placement(Primitives::FlyoutPlacementMode::Bottom);
        flyout.FlyoutPresenterStyle(Resources().Lookup(box_value(L"EdgeToEdgeFlyoutPresenterStyle")).as<winrt::Windows::UI::Xaml::Style>());

        StackPanel content;
        content.Orientation(Orientation::Vertical);
        content.MinWidth(120.0);

        const auto separatorTemplate = Resources().Lookup(box_value(L"ViewAllKeyChordsFlyoutSeparatorTemplate")).as<DataTemplate>();
        const auto itemTemplate = Resources().Lookup(box_value(L"ViewAllKeyChordsFlyoutItemTemplate")).as<DataTemplate>();
        const auto chords = cmdVM.KeyChordList();
        const auto count = chords.Size();
        // Skip index 0 — the first chord is shown inline on the row, not in the flyout.
        for (uint32_t i = 1; i < count; ++i)
        {
            if (i > 1)
            {
                content.Children().Append(separatorTemplate.LoadContent().as<UIElement>());
            }

            auto chordVisual = itemTemplate.LoadContent().as<Editor::KeyChordVisual>();
            chordVisual.KeyChord(chords.GetAt(i).CurrentKeys());
            content.Children().Append(chordVisual);
        }

        flyout.Content(content);
        button.Flyout(flyout);
        flyout.ShowAt(button);
    }
}
