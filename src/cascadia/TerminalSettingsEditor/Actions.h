// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
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

    struct ModifyKeyBindingEventArgs : ModifyKeyBindingEventArgsT<ModifyKeyBindingEventArgs>
    {
    public:
        ModifyKeyBindingEventArgs(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys, const hstring oldActionName, const hstring newActionName) :
            _OldKeys{ oldKeys },
            _NewKeys{ newKeys },
            _OldActionName{ std::move(oldActionName) },
            _NewActionName{ std::move(newActionName) } {}

        WINRT_PROPERTY(Control::KeyChord, OldKeys, nullptr);
        WINRT_PROPERTY(Control::KeyChord, NewKeys, nullptr);
        WINRT_PROPERTY(hstring, OldActionName);
        WINRT_PROPERTY(hstring, NewActionName);
    };

    struct KeyBindingViewModel : KeyBindingViewModelT<KeyBindingViewModel>, ViewModelHelper<KeyBindingViewModel>
    {
    public:
        KeyBindingViewModel(const Windows::Foundation::Collections::IObservableVector<hstring>& availableActions);
        KeyBindingViewModel(const Control::KeyChord& keys, const hstring& name, const Windows::Foundation::Collections::IObservableVector<hstring>& availableActions);

        hstring Name() const { return _CurrentAction; }
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
        void AttemptAcceptChanges(const Control::KeyChord newKeys);
        void CancelChanges();
        void DeleteKeyBinding() { _DeleteKeyBindingRequestedHandlers(*this, _CurrentKeys); }

        // ProposedAction:   the entry selected by the combo box; may disagree with the settings model.
        // CurrentAction:    the combo box item that maps to the settings model value.
        // AvailableActions: the list of options in the combo box; both actions above must be in this list.
        // NOTE: ProposedAction and CurrentAction may disagree mainly due to the "edit mode" system in place.
        //       Current Action serves as...
        //       1 - a record of what to set ProposedAction to on a cancellation
        //       2 - a form of translation between ProposedAction and the settings model
        //       We would also need an ActionMap reference to remove this, but this is a better separation
        //       of responsibilities.
        VIEW_MODEL_OBSERVABLE_PROPERTY(IInspectable, ProposedAction);
        VIEW_MODEL_OBSERVABLE_PROPERTY(hstring, CurrentAction);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<hstring>, AvailableActions, nullptr);

        // ProposedKeys: the keys proposed by the control; may disagree with the settings model.
        // CurrentKeys:  the key chord bound in the settings model.
        VIEW_MODEL_OBSERVABLE_PROPERTY(Control::KeyChord, ProposedKeys);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Control::KeyChord, CurrentKeys, nullptr);

        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsInEditMode, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsNewlyAdded, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::Flyout, AcceptChangesFlyout, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsAutomationPeerAttached, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsHovered, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsContainerFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsEditButtonFocused, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Media::Brush, ContainerBackground, nullptr);
        TYPED_EVENT(ModifyKeyBindingRequested, Editor::KeyBindingViewModel, Editor::ModifyKeyBindingEventArgs);
        TYPED_EVENT(DeleteKeyBindingRequested, Editor::KeyBindingViewModel, Terminal::Control::KeyChord);
        TYPED_EVENT(DeleteNewlyAddedKeyBinding, Editor::KeyBindingViewModel, IInspectable);

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

    struct Actions : public HasScrollViewer<Actions>, ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        void AddNew_Click(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ActionsPageNavigationState, State, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::KeyBindingViewModel>, KeyBindingList);

    private:
        void _ViewModelPropertyChangedHandler(const Windows::Foundation::IInspectable& senderVM, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
        void _ViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Control::KeyChord& args);
        void _ViewModelModifyKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::ModifyKeyBindingEventArgs& args);
        void _ViewModelDeleteNewlyAddedKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const IInspectable& args);

        std::optional<uint32_t> _GetContainerIndexByKeyChord(const Control::KeyChord& keys);
        void _RegisterEvents(com_ptr<implementation::KeyBindingViewModel>& kbdVM);

        bool _AutomationPeerAttached{ false };
        Windows::Foundation::Collections::IObservableVector<hstring> _AvailableActionAndArgs;
        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionMap;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
