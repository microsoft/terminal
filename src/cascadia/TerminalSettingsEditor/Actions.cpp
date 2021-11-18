// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "KeyBindingViewModel.g.cpp"
#include "ActionsPageNavigationState.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    KeyBindingViewModel::KeyBindingViewModel(const Windows::Foundation::Collections::IObservableVector<hstring>& availableActions) :
        KeyBindingViewModel(nullptr, availableActions.First().Current(), availableActions) {}

    KeyBindingViewModel::KeyBindingViewModel(const Control::KeyChord& keys, const hstring& actionName, const IObservableVector<hstring>& availableActions) :
        _CurrentKeys{ keys },
        _KeyChordText{ KeyChordSerialization::ToString(keys) },
        _CurrentAction{ actionName },
        _ProposedAction{ box_value(actionName) },
        _AvailableActions{ availableActions }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"CurrentKeys")
            {
                _KeyChordText = KeyChordSerialization::ToString(_CurrentKeys);
                _NotifyChanges(L"KeyChordText");
            }
            else if (viewModelProperty == L"IsContainerFocused" ||
                     viewModelProperty == L"IsEditButtonFocused" ||
                     viewModelProperty == L"IsHovered" ||
                     viewModelProperty == L"IsAutomationPeerAttached" ||
                     viewModelProperty == L"IsInEditMode")
            {
                _NotifyChanges(L"ShowEditButton");
            }
            else if (viewModelProperty == L"CurrentAction")
            {
                _NotifyChanges(L"Name");
            }
        });
    }

    hstring KeyBindingViewModel::EditButtonName() const noexcept { return RS_(L"Actions_EditButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }
    hstring KeyBindingViewModel::CancelButtonName() const noexcept { return RS_(L"Actions_CancelButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }
    hstring KeyBindingViewModel::AcceptButtonName() const noexcept { return RS_(L"Actions_AcceptButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }
    hstring KeyBindingViewModel::DeleteButtonName() const noexcept { return RS_(L"Actions_DeleteButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }

    bool KeyBindingViewModel::ShowEditButton() const noexcept
    {
        return (IsContainerFocused() || IsEditButtonFocused() || IsHovered() || IsAutomationPeerAttached()) && !IsInEditMode();
    }

    void KeyBindingViewModel::ToggleEditMode()
    {
        // toggle edit mode
        IsInEditMode(!_IsInEditMode);
        if (_IsInEditMode)
        {
            // if we're in edit mode,
            // - pre-populate the text box with the current keys
            // - reset the combo box with the current action
            ProposedKeys(_CurrentKeys);
            ProposedAction(box_value(_CurrentAction));
        }
    }

    void KeyBindingViewModel::AttemptAcceptChanges()
    {
        AttemptAcceptChanges(_ProposedKeys);
    }

    void KeyBindingViewModel::AttemptAcceptChanges(const Control::KeyChord newKeys)
    {
        const auto args{ make_self<ModifyKeyBindingEventArgs>(_CurrentKeys, // OldKeys
                                                              newKeys, // NewKeys
                                                              _IsNewlyAdded ? hstring{} : _CurrentAction, // OldAction
                                                              unbox_value<hstring>(_ProposedAction)) }; // NewAction
        _ModifyKeyBindingRequestedHandlers(*this, *args);
    }

    void KeyBindingViewModel::CancelChanges()
    {
        if (_IsNewlyAdded)
        {
            _DeleteNewlyAddedKeyBindingHandlers(*this, nullptr);
        }
        else
        {
            ToggleEditMode();
        }
    }

    Actions::Actions()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"Actions_AddNewTextBlock/Text"));
    }

    Automation::Peers::AutomationPeer Actions::OnCreateAutomationPeer()
    {
        _AutomationPeerAttached = true;
        for (const auto& kbdVM : _KeyBindingList)
        {
            // To create a more accessible experience, we want the "edit" buttons to _always_
            // appear when a screen reader is attached. This ensures that the edit buttons are
            // accessible via the UIA tree.
            get_self<KeyBindingViewModel>(kbdVM)->IsAutomationPeerAttached(_AutomationPeerAttached);
        }
        return nullptr;
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ActionsPageNavigationState>();

        // Populate AvailableActionAndArgs
        _AvailableActionMap = single_threaded_map<hstring, Model::ActionAndArgs>();
        std::vector<hstring> availableActionAndArgs;
        for (const auto& [name, actionAndArgs] : _State.Settings().ActionMap().AvailableActions())
        {
            availableActionAndArgs.push_back(name);
            _AvailableActionMap.Insert(name, actionAndArgs);
        }
        std::sort(begin(availableActionAndArgs), end(availableActionAndArgs));
        _AvailableActionAndArgs = single_threaded_observable_vector(std::move(availableActionAndArgs));

        // Convert the key bindings from our settings into a view model representation
        const auto& keyBindingMap{ _State.Settings().ActionMap().KeyBindings() };
        std::vector<Editor::KeyBindingViewModel> keyBindingList;
        keyBindingList.reserve(keyBindingMap.Size());
        for (const auto& [keys, cmd] : keyBindingMap)
        {
            // convert the cmd into a KeyBindingViewModel
            auto container{ make_self<KeyBindingViewModel>(keys, cmd.Name(), _AvailableActionAndArgs) };
            _RegisterEvents(container);
            keyBindingList.push_back(*container);
        }

        std::sort(begin(keyBindingList), end(keyBindingList), KeyBindingViewModelComparator{});
        _KeyBindingList = single_threaded_observable_vector(std::move(keyBindingList));
    }

    void Actions::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
    {
        // Create the new key binding and register all of the event handlers.
        auto kbdVM{ make_self<KeyBindingViewModel>(_AvailableActionAndArgs) };
        _RegisterEvents(kbdVM);
        kbdVM->DeleteNewlyAddedKeyBinding({ this, &Actions::_ViewModelDeleteNewlyAddedKeyBindingHandler });

        // Manually add the editing background. This needs to be done in Actions not the view model.
        // We also have to do this manually because it hasn't been added to the list yet.
        kbdVM->IsInEditMode(true);
        const auto& containerBackground{ Resources().Lookup(box_value(L"ActionContainerBackgroundEditing")).as<Windows::UI::Xaml::Media::Brush>() };
        kbdVM->ContainerBackground(containerBackground);

        // IMPORTANT: do this _after_ setting IsInEditMode. Otherwise, it'll get deleted immediately
        //              by the PropertyChangedHandler below (where we delete any IsNewlyAdded items)
        kbdVM->IsNewlyAdded(true);
        _KeyBindingList.InsertAt(0, *kbdVM);
    }

    void Actions::_ViewModelPropertyChangedHandler(const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args)
    {
        const auto senderVM{ sender.as<Editor::KeyBindingViewModel>() };
        const auto propertyName{ args.PropertyName() };
        if (propertyName == L"IsInEditMode")
        {
            if (senderVM.IsInEditMode())
            {
                // Ensure that...
                // 1. we move focus to the edit mode controls
                // 2. any actions that were newly added are removed
                // 3. this is the only entry that is in edit mode
                for (int32_t i = _KeyBindingList.Size() - 1; i >= 0; --i)
                {
                    const auto& kbdVM{ _KeyBindingList.GetAt(i) };
                    if (senderVM == kbdVM)
                    {
                        // This is the view model entry that went into edit mode.
                        // Move focus to the edit mode controls by
                        // extracting the list view item container.
                        const auto& container{ KeyBindingsListView().ContainerFromIndex(i).try_as<ListViewItem>() };
                        container.Focus(FocusState::Programmatic);
                    }
                    else if (kbdVM.IsNewlyAdded())
                    {
                        // Remove any actions that were newly added
                        _KeyBindingList.RemoveAt(i);
                    }
                    else
                    {
                        // Exit edit mode for all other containers
                        get_self<KeyBindingViewModel>(kbdVM)->DisableEditMode();
                    }
                }

                const auto& containerBackground{ Resources().Lookup(box_value(L"ActionContainerBackgroundEditing")).as<Windows::UI::Xaml::Media::Brush>() };
                get_self<KeyBindingViewModel>(senderVM)->ContainerBackground(containerBackground);
            }
            else
            {
                // Focus on the list view item
                KeyBindingsListView().ContainerFromItem(senderVM).as<Controls::Control>().Focus(FocusState::Programmatic);

                const auto& containerBackground{ Resources().Lookup(box_value(L"ActionContainerBackground")).as<Windows::UI::Xaml::Media::Brush>() };
                get_self<KeyBindingViewModel>(senderVM)->ContainerBackground(containerBackground);
            }
        }
    }

    void Actions::_ViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Control::KeyChord& keys)
    {
        // Update the settings model
        _State.Settings().ActionMap().DeleteKeyBinding(keys);

        // Find the current container in our list and remove it.
        // This is much faster than rebuilding the entire ActionMap.
        uint32_t index;
        if (_KeyBindingList.IndexOf(senderVM, index))
        {
            _KeyBindingList.RemoveAt(index);

            // Focus the new item at this index
            if (_KeyBindingList.Size() != 0)
            {
                const auto newFocusedIndex{ std::clamp(index, 0u, _KeyBindingList.Size() - 1) };
                KeyBindingsListView().ContainerFromIndex(newFocusedIndex).as<Controls::Control>().Focus(FocusState::Programmatic);
            }
        }
    }

    void Actions::_ViewModelModifyKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::ModifyKeyBindingEventArgs& args)
    {
        const auto isNewAction{ !args.OldKeys() && args.OldActionName().empty() };

        auto applyChangesToSettingsModel = [=]() {
            // If the key chord was changed,
            // update the settings model and view model appropriately
            // NOTE: we still need to update the view model if we're working with a newly added action
            if (isNewAction || args.OldKeys().Modifiers() != args.NewKeys().Modifiers() || args.OldKeys().Vkey() != args.NewKeys().Vkey())
            {
                if (!isNewAction)
                {
                    // update settings model
                    _State.Settings().ActionMap().RebindKeys(args.OldKeys(), args.NewKeys());
                }

                // update view model
                auto senderVMImpl{ get_self<KeyBindingViewModel>(senderVM) };
                senderVMImpl->CurrentKeys(args.NewKeys());
            }

            // If the action was changed,
            // update the settings model and view model appropriately
            // NOTE: no need to check for "isNewAction" here. <empty_string> != <action name> already.
            if (args.OldActionName() != args.NewActionName())
            {
                // convert the action's name into a view model.
                const auto& newAction{ _AvailableActionMap.Lookup(args.NewActionName()) };

                // update settings model
                _State.Settings().ActionMap().RegisterKeyBinding(args.NewKeys(), newAction);

                // update view model
                auto senderVMImpl{ get_self<KeyBindingViewModel>(senderVM) };
                senderVMImpl->CurrentAction(args.NewActionName());
                senderVMImpl->IsNewlyAdded(false);
            }
        };

        // Check for this special case:
        //  we're changing the key chord,
        //  but the new key chord is already in use
        if (isNewAction || args.OldKeys().Modifiers() != args.NewKeys().Modifiers() || args.OldKeys().Vkey() != args.NewKeys().Vkey())
        {
            const auto& conflictingCmd{ _State.Settings().ActionMap().GetActionByKeyChord(args.NewKeys()) };
            if (conflictingCmd)
            {
                // We're about to overwrite another key chord.
                // Display a confirmation dialog.
                TextBlock errorMessageTB{};
                errorMessageTB.Text(RS_(L"Actions_RenameConflictConfirmationMessage"));

                const auto conflictingCmdName{ conflictingCmd.Name() };
                TextBlock conflictingCommandNameTB{};
                conflictingCommandNameTB.Text(fmt::format(L"\"{}\"", conflictingCmdName.empty() ? RS_(L"Actions_UnnamedCommandName") : conflictingCmdName));
                conflictingCommandNameTB.FontStyle(Windows::UI::Text::FontStyle::Italic);

                TextBlock confirmationQuestionTB{};
                confirmationQuestionTB.Text(RS_(L"Actions_RenameConflictConfirmationQuestion"));

                Button acceptBTN{};
                acceptBTN.Content(box_value(RS_(L"Actions_RenameConflictConfirmationAcceptButton")));
                acceptBTN.Click([=](auto&, auto&) {
                    // remove conflicting key binding from list view
                    const auto containerIndex{ _GetContainerIndexByKeyChord(args.NewKeys()) };
                    _KeyBindingList.RemoveAt(*containerIndex);

                    // remove flyout
                    senderVM.AcceptChangesFlyout().Hide();
                    senderVM.AcceptChangesFlyout(nullptr);

                    // update settings model and view model
                    applyChangesToSettingsModel();
                    senderVM.ToggleEditMode();
                });

                StackPanel flyoutStack{};
                flyoutStack.Children().Append(errorMessageTB);
                flyoutStack.Children().Append(conflictingCommandNameTB);
                flyoutStack.Children().Append(confirmationQuestionTB);
                flyoutStack.Children().Append(acceptBTN);

                Flyout acceptChangesFlyout{};
                acceptChangesFlyout.Content(flyoutStack);
                senderVM.AcceptChangesFlyout(acceptChangesFlyout);
                return;
            }
        }

        // update settings model and view model
        applyChangesToSettingsModel();

        // We NEED to toggle the edit mode here,
        // so that if nothing changed, we still exit
        // edit mode.
        senderVM.ToggleEditMode();
    }

    void Actions::_ViewModelDeleteNewlyAddedKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const IInspectable& /*args*/)
    {
        for (uint32_t i = 0; i < _KeyBindingList.Size(); ++i)
        {
            const auto& kbdVM{ _KeyBindingList.GetAt(i) };
            if (kbdVM == senderVM)
            {
                _KeyBindingList.RemoveAt(i);
                return;
            }
        }
    }

    // Method Description:
    // - performs a search on KeyBindingList by key chord.
    // Arguments:
    // - keys - the associated key chord of the command we're looking for
    // Return Value:
    // - the index of the view model referencing the command. If the command doesn't exist, nullopt
    std::optional<uint32_t> Actions::_GetContainerIndexByKeyChord(const Control::KeyChord& keys)
    {
        for (uint32_t i = 0; i < _KeyBindingList.Size(); ++i)
        {
            const auto kbdVM{ get_self<KeyBindingViewModel>(_KeyBindingList.GetAt(i)) };
            const auto& otherKeys{ kbdVM->CurrentKeys() };
            if (otherKeys && keys.Modifiers() == otherKeys.Modifiers() && keys.Vkey() == otherKeys.Vkey())
            {
                return i;
            }
        }

        // TODO GH #6900:
        //  an expedited search can be done if we use cmd.Name()
        //  to quickly search through the sorted list.
        return std::nullopt;
    }

    void Actions::_RegisterEvents(com_ptr<KeyBindingViewModel>& kbdVM)
    {
        kbdVM->PropertyChanged({ this, &Actions::_ViewModelPropertyChangedHandler });
        kbdVM->DeleteKeyBindingRequested({ this, &Actions::_ViewModelDeleteKeyBindingHandler });
        kbdVM->ModifyKeyBindingRequested({ this, &Actions::_ViewModelModifyKeyBindingHandler });
        kbdVM->IsAutomationPeerAttached(_AutomationPeerAttached);
    }
}
