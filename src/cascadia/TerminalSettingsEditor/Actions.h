// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
#include "KeyBindingViewModel.g.h"
#include "ActionsPageNavigationState.g.h"
#include "RebindKeysEventArgs.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct KeyBindingViewModelComparator
    {
        bool operator()(const Editor::KeyBindingViewModel& lhs, const Editor::KeyBindingViewModel& rhs) const
        {
            return lhs.Name() < rhs.Name();
        }
    };

    struct RebindKeysEventArgs : RebindKeysEventArgsT<RebindKeysEventArgs>
    {
    public:
        RebindKeysEventArgs(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys) :
            _OldKeys{ oldKeys },
            _NewKeys{ newKeys } {}

        WINRT_PROPERTY(Control::KeyChord, OldKeys, nullptr);
        WINRT_PROPERTY(Control::KeyChord, NewKeys, nullptr);
    };

    struct KeyBindingViewModel : KeyBindingViewModelT<KeyBindingViewModel>, ViewModelHelper<KeyBindingViewModel>
    {
    public:
        KeyBindingViewModel(const Control::KeyChord& keys, const Settings::Model::Command& cmd);

        hstring Name() const { return _Command.Name(); }
        hstring KeyChordText() const { return _KeyChordText; }
        Settings::Model::Command Command() const { return _Command; };

        // UIA Text
        hstring EditButtonName() const noexcept;
        hstring CancelButtonName() const noexcept;
        hstring AcceptButtonName() const noexcept;
        hstring DeleteButtonName() const noexcept;

        void EnterHoverMode() { IsHovered(true); };
        void ExitHoverMode() { IsHovered(false); };
        void FocusContainer() { IsContainerFocused(true); };
        void UnfocusContainer() { IsContainerFocused(false); };
        void FocusEditButton() { IsEditButtonFocused(true); };
        void UnfocusEditButton() { IsEditButtonFocused(false); };
        bool ShowEditButton() const noexcept;
        void ToggleEditMode();
        void DisableEditMode() { IsInEditMode(false); }
        void AttemptAcceptChanges();
        void AttemptAcceptChanges(hstring newKeyChordText);
        void DeleteKeyBinding() { _DeleteKeyBindingRequestedHandlers(*this, _Keys); }

        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsInEditMode, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProposedKeys);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Control::KeyChord, Keys, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::Flyout, AcceptChangesFlyout, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsAutomationPeerAttached, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsHovered, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsContainerFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsEditButtonFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Media::Brush, ContainerBackground, nullptr);
        TYPED_EVENT(RebindKeysRequested, Editor::KeyBindingViewModel, Editor::RebindKeysEventArgs);
        TYPED_EVENT(DeleteKeyBindingRequested, Editor::KeyBindingViewModel, Terminal::Control::KeyChord);

    private:
        Settings::Model::Command _Command{ nullptr };
        hstring _KeyChordText{};
    };

    struct ActionsPageNavigationState : ActionsPageNavigationStateT<ActionsPageNavigationState>
    {
    public:
        ActionsPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr)
    };

    struct Actions : ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        void KeyChordEditor_PreviewKeyDown(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::KeyBindingViewModel>, KeyBindingList);

    private:
        void _ViewModelPropertyChangedHandler(const Windows::Foundation::IInspectable& senderVM, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
        void _ViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Control::KeyChord& args);
        void _ViewModelRebindKeysHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::RebindKeysEventArgs& args);

        std::optional<uint32_t> _GetContainerIndexByKeyChord(const Control::KeyChord& keys);

        bool _AutomationPeerAttached{ false };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
