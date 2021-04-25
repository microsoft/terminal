// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
#include "ActionAndArgsViewModel.g.h"
#include "KeyBindingViewModel.g.h"
#include "ActionsPageNavigationState.g.h"
#include "ModifyKeyBindingEventArgs.g.h"
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

    struct ActionAndArgsViewModelComparator
    {
        bool operator()(const Editor::ActionAndArgsViewModel& lhs, const Editor::ActionAndArgsViewModel& rhs) const
        {
            return lhs.Name() < rhs.Name();
        }
    };

    struct ModifyKeyBindingEventArgs : ModifyKeyBindingEventArgsT<ModifyKeyBindingEventArgs>
    {
    public:
        ModifyKeyBindingEventArgs(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys, const Editor::ActionAndArgsViewModel& oldActionVM, const Editor::ActionAndArgsViewModel& newActionVM) :
            _OldKeys{ oldKeys },
            _NewKeys{ newKeys },
            _OldActionVM{ oldActionVM },
            _NewActionVM{ newActionVM } {}

        WINRT_PROPERTY(Control::KeyChord, OldKeys, nullptr);
        WINRT_PROPERTY(Control::KeyChord, NewKeys, nullptr);
        WINRT_PROPERTY(Editor::ActionAndArgsViewModel, OldActionVM, nullptr);
        WINRT_PROPERTY(Editor::ActionAndArgsViewModel, NewActionVM, nullptr);
    };

    struct ActionAndArgsViewModel : ActionAndArgsViewModelT<ActionAndArgsViewModel>, ViewModelHelper<ActionAndArgsViewModel>
    {
    public:
        ActionAndArgsViewModel(hstring name, const Model::ActionAndArgs& actionData) :
            _Name{ name },
            _ActionData{ actionData } {};

        hstring Name() const noexcept { return _Name; }
        Model::ActionAndArgs ActionData() const noexcept { return _ActionData.Copy(); }

    private:
        hstring _Name;
        Model::ActionAndArgs _ActionData{ nullptr };
    };

    struct KeyBindingViewModel : KeyBindingViewModelT<KeyBindingViewModel>, ViewModelHelper<KeyBindingViewModel>
    {
    public:
        KeyBindingViewModel(const Control::KeyChord& keys, const Editor::ActionAndArgsViewModel& actionViewModel, const Windows::Foundation::Collections::IObservableVector<Editor::ActionAndArgsViewModel>& availableActions);

        hstring Name() const { return _CurrentAction.Name(); }
        hstring KeyChordText() const { return _KeyChordText; }

        // UIA Text
        hstring EditButtonName() const noexcept;
        hstring CancelButtonName() const noexcept;
        hstring AcceptButtonName() const noexcept;
        hstring DeleteButtonName() const noexcept;

        void EnterHoverMode() { IsHovered(true); };
        void ExitHoverMode() { IsHovered(false); };
        void ActionGotFocus() { IsContainerFocused(true); };
        void ActionLostFocus() { IsContainerFocused(false); };
        void EditButtonGettingFocus() { IsEditButtonFocused(true); };
        void EditButtonLosingFocus() { IsEditButtonFocused(false); };
        bool ShowEditButton() const noexcept;
        void ToggleEditMode();
        void DisableEditMode() { IsInEditMode(false); }
        void AttemptAcceptChanges();
        void AttemptAcceptChanges(hstring newKeyChordText);
        void DeleteKeyBinding() { _DeleteKeyBindingRequestedHandlers(*this, _Keys); }

        // ProposedAction:   the entry selected by the combo box; may disagree with the settings model.
        // CurrentAction:    the combo box item that maps to the settings model value.
        // AvailableActions: the list of options in the combo box; both actions above must be in this list.
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ActionAndArgsViewModel, ProposedAction, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ActionAndArgsViewModel, CurrentAction, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ActionAndArgsViewModel>, AvailableActions, nullptr);

        // ProposedKeys: the text shown in the text box; may disagree with the settings model.
        // Keys:         the key chord bound in the settings model.
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, ProposedKeys);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Control::KeyChord, Keys, nullptr);

        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsInEditMode, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::Flyout, AcceptChangesFlyout, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsAutomationPeerAttached, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsHovered, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsContainerFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsEditButtonFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Media::Brush, ContainerBackground, nullptr);
        TYPED_EVENT(ModifyKeyBindingRequested, Editor::KeyBindingViewModel, Editor::ModifyKeyBindingEventArgs);
        TYPED_EVENT(DeleteKeyBindingRequested, Editor::KeyBindingViewModel, Terminal::Control::KeyChord);

    private:
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
        void KeyChordEditor_KeyDown(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::KeyBindingViewModel>, KeyBindingList);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ActionAndArgsViewModel>, AvailableActionAndArgs);

    private:
        void _ViewModelPropertyChangedHandler(const Windows::Foundation::IInspectable& senderVM, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
        void _ViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Control::KeyChord& args);
        void _ViewModelModifyKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::ModifyKeyBindingEventArgs& args);

        std::optional<uint32_t> _GetContainerIndexByKeyChord(const Control::KeyChord& keys);

        bool _AutomationPeerAttached{ false };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
