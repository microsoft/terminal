// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "KeyBindingViewModel.g.cpp"
#include "ActionsPageNavigationState.g.cpp"
#include "LibraryResources.h"

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
    KeyBindingViewModel::KeyBindingViewModel(const Control::KeyChord& keys, const Model::Command& cmd) :
        _Keys{ keys },
        _KeyChordText{ Model::KeyChordSerialization::ToString(keys) },
        _Command{ cmd }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"Keys")
            {
                _KeyChordText = Model::KeyChordSerialization::ToString(_Keys);
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
        });
    }

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
            // pre-populate the text box with the current keys
            _ProposedKeys = KeyChordText();
            _NotifyChanges(L"ProposedKeys");
        }
    }

    void KeyBindingViewModel::AttemptAcceptChanges()
    {
        auto args{ make_self<RebindKeysEventArgs>(_Keys, _Keys) };
        try
        {
            // Attempt to convert the provided key chord text
            const auto newKeyChord{ KeyChordSerialization::FromString(_ProposedKeys) };
            args->NewKeys(newKeyChord);
        }
        catch (hresult_invalid_argument)
        {
            // Converting the text into a key chord failed
            // TODO CARLOS:
            //  This is tricky. I still haven't found a way to reference the
            //  key chord text box. It's hidden behind the data template.
            //  Ideally, some kind of notification would alert the user, but
            //  to make it look nice, we need it to somehow target the text box.
        }
        _RebindKeysRequestedHandlers(*this, *args);
    }

    Actions::Actions()
    {
        InitializeComponent();
    }

    Automation::Peers::AutomationPeer Actions::OnCreateAutomationPeer()
    {
        _AutomationPeerAttached = true;
        for (const auto& kbdVM : _KeyBindingList)
        {
            get_self<KeyBindingViewModel>(kbdVM)->IsAutomationPeerAttached(_AutomationPeerAttached);
        }
        return nullptr;
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ActionsPageNavigationState>();

        // Convert the key bindings from our settings into a view model representation
        const auto& keyBindingMap{ _State.Settings().ActionMap().KeyBindings() };
        std::vector<Editor::KeyBindingViewModel> keyBindingList;
        keyBindingList.reserve(keyBindingMap.Size());
        for (const auto& [keys, cmd] : keyBindingMap)
        {
            auto container{ make_self<KeyBindingViewModel>(keys, cmd) };
            container->PropertyChanged({ this, &Actions::_ViewModelPropertyChangedHandler });
            container->DeleteKeyBindingRequested({ this, &Actions::_ViewModelDeleteKeyBindingHandler });
            container->RebindKeysRequested({ this, &Actions::_ViewModelRebindKeysHandler });
            container->IsAutomationPeerAttached(_AutomationPeerAttached);
            keyBindingList.push_back(*container);
        }

        std::sort(begin(keyBindingList), end(keyBindingList), KeyBindingViewModelComparator{});
        _KeyBindingList = single_threaded_observable_vector(std::move(keyBindingList));
    }

    void Actions::_ViewModelPropertyChangedHandler(const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args)
    {
        const auto senderVM{ sender.as<Editor::KeyBindingViewModel>() };
        const auto propertyName{ args.PropertyName() };
        if (propertyName == L"IsInEditMode")
        {
            if (senderVM.IsInEditMode())
            {
                for (uint32_t i = 0; i < _KeyBindingList.Size(); ++i)
                {
                    const auto& kbdVM{ _KeyBindingList.GetAt(i) };
                    if (senderVM == kbdVM)
                    {
                        // move focus to the edit mode controls
                        const auto& container{ KeyBindingsListView().ContainerFromIndex(i).try_as<ListViewItem>() };
                        container.Focus(FocusState::Programmatic);
                    }
                    else
                    {
                        // exit edit mode for all other containers
                        get_self<KeyBindingViewModel>(kbdVM)->DisableEditMode();
                    }
                }
            }
            else
            {
                // Focus on the list view item
                KeyBindingsListView().ContainerFromItem(senderVM).as<Controls::Control>().Focus(FocusState::Programmatic);
            }
        }
    }

    void Actions::_ViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& /*senderVM*/, const Control::KeyChord& keys)
    {
        // Update the settings model
        _State.Settings().ActionMap().DeleteKeyBinding(keys);

        // Find the current container in our list and remove it.
        // This is much faster than rebuilding the entire ActionMap.
        if (const auto index{ _GetContainerIndexByKeyChord(keys) })
        {
            _KeyBindingList.RemoveAt(*index);

            // Focus the new item at this index
            if (_KeyBindingList.Size() != 0)
            {
                const auto newFocusedIndex{ std::clamp(*index, 0u, _KeyBindingList.Size() - 1) };
                KeyBindingsListView().ContainerFromIndex(newFocusedIndex).as<Controls::Control>().Focus(FocusState::Programmatic);
            }
        }
    }

    void Actions::_ViewModelRebindKeysHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::RebindKeysEventArgs& args)
    {
        if (args.OldKeys().Modifiers() != args.NewKeys().Modifiers() || args.OldKeys().Vkey() != args.NewKeys().Vkey())
        {
            // We're actually changing the key chord
            const auto senderVMImpl{ get_self<KeyBindingViewModel>(senderVM) };
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
                    _State.Settings().ActionMap().RebindKeys(args.OldKeys(), args.NewKeys());
                    senderVMImpl->Keys(args.NewKeys());
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
            else
            {
                // update settings model
                _State.Settings().ActionMap().RebindKeys(args.OldKeys(), args.NewKeys());

                // update view model (keys)
                senderVMImpl->Keys(args.NewKeys());
            }
        }

        // update view model (exit edit mode)
        senderVM.ToggleEditMode();
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
            const auto& otherKeys{ kbdVM->Keys() };
            if (keys.Modifiers() == otherKeys.Modifiers() && keys.Vkey() == otherKeys.Vkey())
            {
                return i;
            }
        }

        // TODO CARLOS:
        //  an expedited search can be done if we use cmd.Name()
        //  to quickly search through the sorted list.
        return std::nullopt;
    }
}
