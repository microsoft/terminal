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
    // Depth-first search of the visual tree under 'root' for the first KeyChordListener.
    static Editor::KeyChordListener _findKeyChordListener(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto listener = root.try_as<Editor::KeyChordListener>())
        {
            return listener;
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = _findKeyChordListener(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    // Depth-first search of the visual tree under 'root' for the first focusable, visible
    // control (e.g. a key chord row's edit pencil), used to restore focus to a row after it
    // leaves inline edit mode.
    static Controls::Control _findFirstFocusable(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto control = root.try_as<Controls::Control>())
        {
            if (control.IsTabStop() && control.IsEnabled() && control.Visibility() == Visibility::Visible)
            {
                return control;
            }
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = _findFirstFocusable(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    // Depth-first search of the visual tree under 'root' for the first ItemsControl. Used to
    // locate a command row's nested key chord list so we can resolve a specific chord's row.
    static Controls::ItemsControl _findChildItemsControl(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto itemsControl = child.try_as<Controls::ItemsControl>())
            {
                return itemsControl;
            }
            if (const auto found = _findChildItemsControl(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    Actions::Actions()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"Actions_AddNewTextBlock/Text"));
    }

    // Called when a key chord row enters or leaves inline edit mode. When entering, focus that
    // row's KeyChordListener so the user can immediately type a chord. When leaving, return
    // focus to the row's first focusable control (the edit pencil).
    void Actions::_FocusKeyChordContainer(const Editor::CommandViewModel& cmdVM, const Editor::KeyChordViewModel& kcVM)
    {
        if (!cmdVM || !kcVM)
        {
            return;
        }

        // Defer to the next dispatcher tick: when entering edit mode the listener's
        // Visibility="{x:Bind IsInEditMode}" binding (and the corresponding layout) has not
        // applied yet, so focusing it synchronously would be a no-op (can't focus a collapsed
        // element). Running after the tick lets visibility/layout settle first.
        auto weakThis{ get_weak() };
        winrt::Windows::System::DispatcherQueue::GetForCurrentThread().TryEnqueue([weakThis, cmdVM, kcVM]() {
            const auto self{ weakThis.get() };
            if (!self)
            {
                return;
            }

            const auto cmdContainer = self->CommandsListView().ContainerFromItem(cmdVM);
            if (!cmdContainer)
            {
                return;
            }
            // Realize the containers in case this row was just added/revealed.
            self->CommandsListView().UpdateLayout();

            const auto cmdRoot = cmdContainer.try_as<DependencyObject>();
            const auto kcItemsControl = _findChildItemsControl(cmdRoot);
            if (!kcItemsControl)
            {
                return;
            }
            kcItemsControl.UpdateLayout();

            const auto rowContainer = kcItemsControl.ContainerFromItem(kcVM);
            if (!rowContainer)
            {
                return;
            }
            const auto rowRoot = rowContainer.try_as<DependencyObject>();

            if (kcVM.IsInEditMode())
            {
                // Focus the editable listener so the user can type a chord.
                if (const auto listener = _findKeyChordListener(rowRoot))
                {
                    listener.FocusInput();
                    return;
                }
            }

            // Otherwise (left edit mode) return focus to the row's first focusable control.
            if (const auto focusable = _findFirstFocusable(rowRoot))
            {
                focusable.Focus(FocusState::Programmatic);
            }
        });
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::ActionsViewModel>();
        _ViewModel.ReSortCommandList();
        _focusKeyChordContainerRevoker = _ViewModel.FocusKeyChordContainerRequested(winrt::auto_revoke, { this, &Actions::_FocusKeyChordContainer });
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
}
