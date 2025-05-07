// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionsViewModel.h"
#include "ActionsViewModel.g.cpp"
#include "KeyBindingViewModel.g.cpp"
#include "CommandViewModel.g.cpp"
#include "ArgWrapper.g.cpp"
#include "ActionArgsViewModel.g.cpp"
#include "KeyChordViewModel.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"
#include "EnumEntry.h"
#include "ColorSchemeViewModel.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

// todo:
//      multiple actions
//      selection color
// the above arg types aren't implemented yet - they all have multiple values within them
// and require a different approach to binding/displaying. Selection color has color and IsIndex16,
// multiple actions is... multiple actions
// for now, do not support these shortcut actions in the new action editor
inline const std::set<winrt::Microsoft::Terminal::Settings::Model::ShortcutAction> UnimplementedShortcutActions = {
    winrt::Microsoft::Terminal::Settings::Model::ShortcutAction::MultipleActions,
    winrt::Microsoft::Terminal::Settings::Model::ShortcutAction::ColorSelection
};

#define INITIALIZE_ENUM_LIST_AND_VALUE(enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                                \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> enumList;                                                                          \
    const auto mappings = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                                    \
    enumType unboxedValue;                                                                                                                                  \
    if (_Value)                                                                                                                                             \
    {                                                                                                                                                       \
        unboxedValue = unbox_value<enumType>(_Value);                                                                                                       \
    }                                                                                                                                                       \
    for (const auto [enumKey, enumValue] : mappings)                                                                                                        \
    {                                                                                                                                                       \
        const auto enumName = LocalizedNameForEnumName(resourceSectionAndType, enumKey, resourceProperty);                                                  \
        auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(enumValue)); \
        enumList.emplace_back(entry);                                                                                                                       \
        if (_Value && unboxedValue == enumValue)                                                                                                            \
        {                                                                                                                                                   \
            _EnumValue = entry;                                                                                                                             \
        }                                                                                                                                                   \
    }                                                                                                                                                       \
    std::sort(enumList.begin(), enumList.end(), EnumEntryReverseComparator<enumType>());                                                                    \
    _EnumList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(enumList));                     \
    _NotifyChanges(L"EnumList", L"EnumValue");

#define INITIALIZE_NULLABLE_ENUM_LIST_AND_VALUE(enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                       \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> enumList;                                                                          \
    const auto mappings = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                                    \
    enumType unboxedValue;                                                                                                                                  \
    auto nullEntry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(RS_(L"Actions_NullEnumValue"), nullptr);          \
    if (_Value)                                                                                                                                             \
    {                                                                                                                                                       \
        unboxedValue = unbox_value<enumType>(_Value);                                                                                                       \
    }                                                                                                                                                       \
    else                                                                                                                                                    \
    {                                                                                                                                                       \
        _EnumValue = nullEntry;                                                                                                                             \
    }                                                                                                                                                       \
    for (const auto [enumKey, enumValue] : mappings)                                                                                                        \
    {                                                                                                                                                       \
        const auto enumName = LocalizedNameForEnumName(resourceSectionAndType, enumKey, resourceProperty);                                                  \
        auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(enumValue)); \
        enumList.emplace_back(entry);                                                                                                                       \
        if (_Value && unboxedValue == enumValue)                                                                                                            \
        {                                                                                                                                                   \
            _EnumValue = entry;                                                                                                                             \
        }                                                                                                                                                   \
    }                                                                                                                                                       \
    std::sort(enumList.begin(), enumList.end(), EnumEntryReverseComparator<enumType>());                                                                    \
    enumList.emplace_back(nullEntry);                                                                                                                       \
    _EnumList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(enumList));                     \
    _NotifyChanges(L"EnumList", L"EnumValue");

#define INITIALIZE_FLAG_LIST_AND_VALUE(enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                                           \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry> flagList;                                                                                     \
    const auto mappings = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                                               \
    enumType unboxedValue{ 0 };                                                                                                                                        \
    if (_Value)                                                                                                                                                        \
    {                                                                                                                                                                  \
        unboxedValue = unbox_value<enumType>(_Value);                                                                                                                  \
    }                                                                                                                                                                  \
    for (const auto [flagKey, flagValue] : mappings)                                                                                                                   \
    {                                                                                                                                                                  \
        if (flagKey != L"all" && flagKey != L"none")                                                                                                                   \
        {                                                                                                                                                              \
            const auto flagName = LocalizedNameForEnumName(resourceSectionAndType, flagKey, resourceProperty);                                                         \
            bool isSet = WI_IsAnyFlagSet(unboxedValue, flagValue);                                                                                                     \
            auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(flagName, winrt::box_value<enumType>(flagValue), isSet); \
            entry.PropertyChanged([&, flagValue](const IInspectable& sender, const PropertyChangedEventArgs& args) {                                                   \
                const auto itemProperty{ args.PropertyName() };                                                                                                        \
                if (itemProperty == L"IsSet")                                                                                                                          \
                {                                                                                                                                                      \
                    const auto flagWrapper = sender.as<Microsoft::Terminal::Settings::Editor::FlagEntry>();                                                            \
                    auto unboxed = unbox_value<enumType>(_Value);                                                                                                      \
                    flagWrapper.IsSet() ? WI_SetAllFlags(unboxed, flagValue) : WI_ClearAllFlags(unboxed, flagValue);                                                   \
                    Value(box_value(unboxed));                                                                                                                         \
                }                                                                                                                                                      \
            });                                                                                                                                                        \
            flagList.emplace_back(entry);                                                                                                                              \
        }                                                                                                                                                              \
    }                                                                                                                                                                  \
    std::sort(flagList.begin(), flagList.end(), FlagEntryReverseComparator<enumType>());                                                                               \
    _FlagList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>(std::move(flagList));                                \
    _NotifyChanges(L"FlagList");

#define INITIALIZE_NULLABLE_FLAG_LIST_AND_VALUE(enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                                  \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry> flagList;                                                                                     \
    const auto mappings = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                                               \
    enumType unboxedValue{ 0 };                                                                                                                                        \
    auto nullEntry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(RS_(L"Actions_NullEnumValue"), nullptr, true);               \
    if (_Value)                                                                                                                                                        \
    {                                                                                                                                                                  \
        if (const auto unboxedRef = unbox_value<Windows::Foundation::IReference<enumType>>(_Value))                                                                    \
        {                                                                                                                                                              \
            unboxedValue = unboxedRef.Value();                                                                                                                         \
            nullEntry.IsSet(false);                                                                                                                                    \
        }                                                                                                                                                              \
    }                                                                                                                                                                  \
    for (const auto [flagKey, flagValue] : mappings)                                                                                                                   \
    {                                                                                                                                                                  \
        if (flagKey != L"all" && flagKey != L"none")                                                                                                                   \
        {                                                                                                                                                              \
            const auto flagName = LocalizedNameForEnumName(resourceSectionAndType, flagKey, resourceProperty);                                                         \
            bool isSet = WI_IsAnyFlagSet(unboxedValue, flagValue);                                                                                                     \
            auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(flagName, winrt::box_value<enumType>(flagValue), isSet); \
            entry.PropertyChanged([&, flagValue, nullEntry](const IInspectable& sender, const PropertyChangedEventArgs& args) {                                        \
                const auto itemProperty{ args.PropertyName() };                                                                                                        \
                if (itemProperty == L"IsSet")                                                                                                                          \
                {                                                                                                                                                      \
                    const auto flagWrapper = sender.as<Microsoft::Terminal::Settings::Editor::FlagEntry>();                                                            \
                    enumType unboxedValue{ 0 };                                                                                                                        \
                    auto unboxedRef = unbox_value<Windows::Foundation::IReference<enumType>>(_Value);                                                                  \
                    if (unboxedRef)                                                                                                                                    \
                    {                                                                                                                                                  \
                        unboxedValue = unboxedRef.Value();                                                                                                             \
                    }                                                                                                                                                  \
                    if (flagWrapper.IsSet())                                                                                                                           \
                    {                                                                                                                                                  \
                        nullEntry.IsSet(false);                                                                                                                        \
                        WI_SetAllFlags(unboxedValue, flagValue);                                                                                                       \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        WI_ClearAllFlags(unboxedValue, flagValue);                                                                                                     \
                    }                                                                                                                                                  \
                    Value(box_value(unboxedValue));                                                                                                                    \
                }                                                                                                                                                      \
            });                                                                                                                                                        \
            flagList.emplace_back(entry);                                                                                                                              \
        }                                                                                                                                                              \
    }                                                                                                                                                                  \
    std::sort(flagList.begin(), flagList.end(), FlagEntryReverseComparator<enumType>());                                                                               \
    nullEntry.PropertyChanged([&](const IInspectable& sender, const PropertyChangedEventArgs& args) {                                                                  \
        const auto itemProperty{ args.PropertyName() };                                                                                                                \
        if (itemProperty == L"IsSet")                                                                                                                                  \
        {                                                                                                                                                              \
            const auto flagWrapper = sender.as<Microsoft::Terminal::Settings::Editor::FlagEntry>();                                                                    \
            if (flagWrapper.IsSet())                                                                                                                                   \
            {                                                                                                                                                          \
                for (const auto flagEntry : _FlagList)                                                                                                                 \
                {                                                                                                                                                      \
                    if (flagEntry.FlagName() != RS_(L"Actions_NullEnumValue"))                                                                                         \
                    {                                                                                                                                                  \
                        flagEntry.IsSet(false);                                                                                                                        \
                    }                                                                                                                                                  \
                }                                                                                                                                                      \
                Value(box_value(Windows::Foundation::IReference<enumType>(nullptr)));                                                                                  \
            }                                                                                                                                                          \
            else                                                                                                                                                       \
            {                                                                                                                                                          \
                Value(box_value(Windows::Foundation::IReference<enumType>(enumType{ 0 })));                                                                            \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
    });                                                                                                                                                                \
    flagList.emplace_back(nullEntry);                                                                                                                                  \
    _FlagList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>(std::move(flagList));                                \
    _NotifyChanges(L"FlagList");

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    KeyBindingViewModel::KeyBindingViewModel(const IObservableVector<hstring>& availableActions) :
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
            // - prepopulate the text box with the current keys
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
        ModifyKeyBindingRequested.raise(*this, *args);
    }

    void KeyBindingViewModel::CancelChanges()
    {
        if (_IsNewlyAdded)
        {
            DeleteNewlyAddedKeyBinding.raise(*this, nullptr);
        }
        else
        {
            ToggleEditMode();
        }
    }

    CommandViewModel::CommandViewModel(Command cmd, std::vector<Control::KeyChord> keyChordList, const Editor::ActionsViewModel actionsPageVM, const Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring>& availableShortcutActionsAndNames) :
        _command{ cmd },
        _keyChordList{ keyChordList },
        _actionsPageVM{ actionsPageVM },
        _AvailableActionsAndNamesMap{ availableShortcutActionsAndNames }
    {
    }

    void CommandViewModel::Initialize()
    {
        std::vector<Editor::KeyChordViewModel> keyChordVMs;
        for (const auto keys : _keyChordList)
        {
            auto kcVM{ make<KeyChordViewModel>(keys) };
            _RegisterKeyChordVMEvents(kcVM);
            keyChordVMs.push_back(kcVM);
        }
        _KeyChordViewModelList = single_threaded_observable_vector(std::move(keyChordVMs));

        std::vector<hstring> shortcutActions;
        for (const auto [action, name] : _AvailableActionsAndNamesMap)
        {
            shortcutActions.emplace_back(name);
            _NameToActionMap.emplace(name, action);
        }
        std::sort(shortcutActions.begin(), shortcutActions.end());
        _AvailableShortcutActions = single_threaded_observable_vector(std::move(shortcutActions));

        const auto shortcutActionString = _AvailableActionsAndNamesMap.Lookup(_command.ActionAndArgs().Action());
        ProposedShortcutAction(winrt::box_value(shortcutActionString));
        _CreateAndInitializeActionArgsVMHelper();

        // Add a property changed handler to our own property changed event.
        // This allows us to create a new ActionArgsVM when the shortcut action changes
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"ProposedShortcutAction")
            {
                const auto actionString = unbox_value<hstring>(ProposedShortcutAction());
                const auto actionEnum = _NameToActionMap.at(actionString);
                const auto emptyArgs = CascadiaSettings::GetEmptyArgsForAction(actionEnum);
                // todo: probably need some better default values for empty args
                // eg. for sendInput, where "input" is a required argument, "input" gets set to an empty string which does not satisfy the requirement
                // i.e. if the user hits "save" immediately after switching to sendInput as the action (without adding something to the input field), they'll get an error
                // there are some other cases as well
                Model::ActionAndArgs newActionAndArgs{ actionEnum, emptyArgs };
                _command.ActionAndArgs(newActionAndArgs);
                if (_IsNewCommand)
                {
                    _command.GenerateID();
                }
                else if (!IsUserAction())
                {
                    _ReplaceCommandWithUserCopy(true);
                    return;
                }
                _CreateAndInitializeActionArgsVMHelper();
            }
        });
    }

    winrt::hstring CommandViewModel::DisplayName()
    {
        return _command.Name();
    }

    winrt::hstring CommandViewModel::Name()
    {
        if (_command.HasName())
        {
            return _command.Name();
        }
        return L"";
    }

    void CommandViewModel::Name(const winrt::hstring& newName)
    {
        if (!newName.empty())
        {
            _command.Name(newName);
        }
    }

    winrt::hstring CommandViewModel::ID()
    {
        return _command.ID();
    }

    void CommandViewModel::ID(const winrt::hstring& ID)
    {
        _command.ID(ID);
    }

    bool CommandViewModel::IsUserAction()
    {
        return _command.Origin() == OriginTag::User;
    }

    void CommandViewModel::Edit_Click()
    {
        EditRequested.raise(*this, *this);
    }

    void CommandViewModel::Delete_Click()
    {
        DeleteRequested.raise(*this, *this);
    }

    void CommandViewModel::AddKeybinding_Click()
    {
        auto kbdVM{ make_self<KeyChordViewModel>(nullptr) };
        kbdVM->IsInEditMode(true);
        _RegisterKeyChordVMEvents(*kbdVM);
        KeyChordViewModelList().Append(*kbdVM);
    }

    void CommandViewModel::_RegisterKeyChordVMEvents(Editor::KeyChordViewModel kcVM)
    {
        if (const auto actionsPageVM{ _actionsPageVM.get() })
        {
            const auto id = ID();
            kcVM.AddKeyChordRequested([actionsPageVM, id](const Editor::KeyChordViewModel& sender, const Control::KeyChord& keys) {
                actionsPageVM.AttemptAddOrModifyKeyChord(sender, id, keys, nullptr);
            });
            kcVM.ModifyKeyChordRequested([actionsPageVM, id](const Editor::KeyChordViewModel& sender, const Editor::ModifyKeyChordEventArgs& args) {
                actionsPageVM.AttemptAddOrModifyKeyChord(sender, id, args.NewKeys(), args.OldKeys());
            });
            kcVM.DeleteKeyChordRequested([&, actionsPageVM](const Editor::KeyChordViewModel& sender, const Control::KeyChord& args) {
                _keyChordList.erase(
                    std::remove_if(
                        _keyChordList.begin(),
                        _keyChordList.end(),
                        [&](const Control::KeyChord& kc) { return kc == args; }),
                    _keyChordList.end());
                for (uint32_t i = 0; i < _KeyChordViewModelList.Size(); i++)
                {
                    if (_KeyChordViewModelList.GetAt(i) == sender)
                    {
                        KeyChordViewModelList().RemoveAt(i);
                        break;
                    }
                }
                actionsPageVM.AttemptDeleteKeyChord(args);
            });
        }
    }

    void CommandViewModel::_RegisterActionArgsVMEvents(Editor::ActionArgsViewModel actionArgsVM)
    {
        actionArgsVM.PropagateColorSchemeRequested([weakThis = get_weak()](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
            if (auto weak = weakThis.get())
            {
                if (wrapper)
                {
                    weak->PropagateColorSchemeRequested.raise(*weak, wrapper);
                }
            }
        });
        if (_IsNewCommand)
        {
            // for new commands, make sure we generate a new ID everytime any arg value changes
            actionArgsVM.WrapperValueChanged([weakThis = get_weak()](const IInspectable& /*sender*/, const IInspectable& /*args*/) {
                if (auto weak = weakThis.get())
                {
                    weak->_command.GenerateID();
                }
            });
        }
        else if (!IsUserAction())
        {
            actionArgsVM.WrapperValueChanged([weakThis = get_weak()](const IInspectable& /*sender*/, const IInspectable& /*args*/) {
                if (auto weak = weakThis.get())
                {
                    weak->_ReplaceCommandWithUserCopy(false);
                }
            });
        }
    }

    void CommandViewModel::_ReplaceCommandWithUserCopy(bool reinitialize)
    {
        // the user is attempting to edit an in-box action
        // to handle this, we create a new command with the new values that has the same ID as the in-box action
        // swap out our underlying command with the copy, tell the ActionsVM that the copy needs to be added to the action map
        if (const auto actionsPageVM{ _actionsPageVM.get() })
        {
            const auto newCmd = Model::Command::CopyAsUserCommand(_command);
            _command = newCmd;
            actionsPageVM.AttemptAddCopiedCommand(_command);
            if (reinitialize)
            {
                // full reinitialize needed, recreate the action args VM
                // (this happens when the shortcut action is being changed on an in-box action)
                _CreateAndInitializeActionArgsVMHelper();
            }
            else
            {
                // no need to reinitialize, just swap out the underlying data model
                // (this happens when an additional argument is being changed on an in-box action)
                auto actionArgsVMImpl{ get_self<ActionArgsViewModel>(_ActionArgsVM) };
                actionArgsVMImpl->ReplaceActionAndArgs(_command.ActionAndArgs());
            }
        }
    }

    void CommandViewModel::_CreateAndInitializeActionArgsVMHelper()
    {
        const auto actionArgsVM = make_self<ActionArgsViewModel>(_command.ActionAndArgs());
        _RegisterActionArgsVMEvents(*actionArgsVM);
        actionArgsVM->Initialize();
        ActionArgsVM(*actionArgsVM);
        _NotifyChanges(L"DisplayName");
    }

    ArgWrapper::ArgWrapper(const winrt::hstring& name, const winrt::hstring& type, const bool required, const Windows::Foundation::IInspectable& value) :
        _name{ name },
        _type{ type },
        _required{ required }
    {
        Value(value);
    }

    void ArgWrapper::Initialize()
    {
        if (_type == L"Model::ResizeDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(ResizeDirection, Model::ResizeDirection, L"Actions_ResizeDirection", L"Content");
        }
        else if (_type == L"Model::FocusDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(FocusDirection, Model::FocusDirection, L"Actions_FocusDirection", L"Content");
        }
        else if (_type == L"SettingsTarget")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(SettingsTarget, Model::SettingsTarget, L"Actions_SettingsTarget", L"Content");
        }
        else if (_type == L"MoveTabDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(MoveTabDirection, Model::MoveTabDirection, L"Actions_MoveTabDirection", L"Content");
        }
        else if (_type == L"Microsoft::Terminal::Control::ScrollToMarkDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(ScrollToMarkDirection, Control::ScrollToMarkDirection, L"Actions_ScrollToMarkDirection", L"Content");
        }
        else if (_type == L"CommandPaletteLaunchMode")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(CommandPaletteLaunchMode, Model::CommandPaletteLaunchMode, L"Actions_CommandPaletteLaunchMode", L"Content");
        }
        else if (_type == L"SuggestionsSource")
        {
            INITIALIZE_FLAG_LIST_AND_VALUE(SuggestionsSource, Model::SuggestionsSource, L"Actions_SuggestionsSource", L"Content");
        }
        else if (_type == L"FindMatchDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(FindMatchDirection, Model::FindMatchDirection, L"Actions_FindMatchDirection", L"Content");
        }
        else if (_type == L"Model::DesktopBehavior")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(DesktopBehavior, Model::DesktopBehavior, L"Actions_DesktopBehavior", L"Content");
        }
        else if (_type == L"Model::MonitorBehavior")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(MonitorBehavior, Model::MonitorBehavior, L"Actions_MonitorBehavior", L"Content");
        }
        else if (_type == L"winrt::Microsoft::Terminal::Control::ClearBufferType")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(ClearBufferType, Control::ClearBufferType, L"Actions_ClearBufferType", L"Content");
        }
        else if (_type == L"SelectOutputDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(SelectOutputDirection, Model::SelectOutputDirection, L"Actions_SelectOutputDirection", L"Content");
        }
        else if (_type == L"Model::SplitDirection")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(SplitDirection, Model::SplitDirection, L"Actions_SplitDirection", L"Content");
        }
        else if (_type == L"SplitType")
        {
            INITIALIZE_ENUM_LIST_AND_VALUE(SplitType, Model::SplitType, L"Actions_SplitType", L"Content");
        }
        else if (_type == L"Windows::Foundation::IReference<TabSwitcherMode>")
        {
            INITIALIZE_NULLABLE_ENUM_LIST_AND_VALUE(TabSwitcherMode, Model::TabSwitcherMode, L"Actions_TabSwitcherMode", L"Content");
        }
        else if (_type == L"Windows::Foundation::IReference<Control::CopyFormat>")
        {
            INITIALIZE_NULLABLE_FLAG_LIST_AND_VALUE(CopyFormat, Control::CopyFormat, L"Actions_CopyFormat", L"Content");
        }
        else if (_type == L"Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>" ||
                 _type == L"Windows::Foundation::IReference<Windows::UI::Color>")
        {
            ColorSchemeRequested.raise(*this, *this);
        }
    }

    void ArgWrapper::EnumValue(const Windows::Foundation::IInspectable& enumValue)
    {
        if (_EnumValue != enumValue)
        {
            _EnumValue = enumValue;
            Value(_EnumValue.as<Editor::EnumEntry>().EnumValue());
        }
    }

    winrt::hstring ArgWrapper::UnboxString(const Windows::Foundation::IInspectable& value)
    {
        return winrt::unbox_value<winrt::hstring>(value);
    }

    winrt::hstring ArgWrapper::UnboxGuid(const Windows::Foundation::IInspectable& value)
    {
        return winrt::to_hstring(winrt::unbox_value<winrt::guid>(value));
    }

    int32_t ArgWrapper::UnboxInt32(const Windows::Foundation::IInspectable& value)
    {
        return winrt::unbox_value<int32_t>(value);
    }

    float ArgWrapper::UnboxInt32Optional(const Windows::Foundation::IInspectable& value)
    {
        const auto unboxed = winrt::unbox_value<winrt::Windows::Foundation::IReference<int32_t>>(value);
        if (unboxed)
        {
            return static_cast<float>(unboxed.Value());
        }
        else
        {
            return NAN;
        }
    }

    uint32_t ArgWrapper::UnboxUInt32(const Windows::Foundation::IInspectable& value)
    {
        return winrt::unbox_value<uint32_t>(value);
    }

    float ArgWrapper::UnboxUInt32Optional(const Windows::Foundation::IInspectable& value)
    {
        const auto unboxed = winrt::unbox_value<winrt::Windows::Foundation::IReference<uint32_t>>(value);
        if (unboxed)
        {
            return static_cast<float>(unboxed.Value());
        }
        else
        {
            return NAN;
        }
    }

    float ArgWrapper::UnboxUInt64(const Windows::Foundation::IInspectable& value)
    {
        return static_cast<float>(winrt::unbox_value<uint64_t>(value));
    }

    float ArgWrapper::UnboxFloat(const Windows::Foundation::IInspectable& value)
    {
        return winrt::unbox_value<float>(value);
    }

    bool ArgWrapper::UnboxBool(const Windows::Foundation::IInspectable& value)
    {
        return winrt::unbox_value<bool>(value);
    }

    winrt::Windows::Foundation::IReference<bool> ArgWrapper::UnboxBoolOptional(const Windows::Foundation::IInspectable& value)
    {
        if (!value)
        {
            return nullptr;
        }
        return winrt::unbox_value<winrt::Windows::Foundation::IReference<bool>>(value);
    }

    winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> ArgWrapper::UnboxTerminalCoreColorOptional(const Windows::Foundation::IInspectable& value)
    {
        if (value)
        {
            return unbox_value<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>(value);
        }
        else
        {
            return nullptr;
        }
    }

    winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> ArgWrapper::UnboxWindowsUIColorOptional(const Windows::Foundation::IInspectable& value)
    {
        if (value)
        {
            const auto winUIColor = unbox_value<Windows::Foundation::IReference<Windows::UI::Color>>(value).Value();
            const Microsoft::Terminal::Core::Color terminalColor{ winUIColor.R, winUIColor.G, winUIColor.B, winUIColor.A };
            return Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>{ terminalColor };
        }
        else
        {
            return nullptr;
        }
    }

    void ArgWrapper::StringBindBack(const winrt::hstring& newValue)
    {
        if (UnboxString(_Value) != newValue)
        {
            Value(box_value(newValue));
        }
    }

    void ArgWrapper::GuidBindBack(const winrt::hstring& newValue)
    {
        if (UnboxGuid(_Value) != newValue)
        {
            // todo: probably need some validation?
            Value(box_value(winrt::guid{ newValue }));
        }
    }

    void ArgWrapper::Int32BindBack(const double newValue)
    {
        if (UnboxInt32(_Value) != newValue)
        {
            Value(box_value(static_cast<int32_t>(newValue)));
        }
    }

    void ArgWrapper::Int32OptionalBindBack(const double newValue)
    {
        if (!isnan(newValue))
        {
            const auto currentValue = UnboxInt32Optional(_Value);
            if (isnan(currentValue) || static_cast<int32_t>(currentValue) != static_cast<int32_t>(newValue))
            {
                Value(box_value(static_cast<int32_t>(newValue)));
            }
        }
        else if (!_Value)
        {
            Value(nullptr);
        }
    }

    void ArgWrapper::UInt32BindBack(const double newValue)
    {
        if (UnboxUInt32(_Value) != newValue)
        {
            Value(box_value(static_cast<uint32_t>(newValue)));
        }
    }

    void ArgWrapper::UInt32OptionalBindBack(const double newValue)
    {
        if (!isnan(newValue))
        {
            const auto currentValue = UnboxUInt32Optional(_Value);
            if (isnan(currentValue) || static_cast<uint32_t>(currentValue) != static_cast<uint32_t>(newValue))
            {
                Value(box_value(static_cast<uint32_t>(newValue)));
            }
        }
        else if (!_Value)
        {
            Value(nullptr);
        }
    }

    void ArgWrapper::UInt64BindBack(const double newValue)
    {
        if (UnboxUInt64(_Value) != newValue)
        {
            Value(box_value(static_cast<uint64_t>(newValue)));
        }
    }

    void ArgWrapper::FloatBindBack(const double newValue)
    {
        if (UnboxFloat(_Value) != newValue)
        {
            Value(box_value(static_cast<float>(newValue)));
        }
    }

    void ArgWrapper::BoolOptionalBindBack(const Windows::Foundation::IReference<bool> newValue)
    {
        if (newValue)
        {
            const auto currentValue = UnboxBoolOptional(_Value);
            if (!currentValue || currentValue.Value() != newValue.Value())
            {
                Value(box_value(newValue));
            }
        }
        else if (_Value)
        {
            Value(nullptr);
        }
    }

    void ArgWrapper::TerminalCoreColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue)
    {
        if (newValue)
        {
            const auto currentValue = UnboxTerminalCoreColorOptional(_Value);
            if (!currentValue || currentValue.Value() != newValue.Value())
            {
                Value(box_value(newValue));
            }
        }
        else if (_Value)
        {
            Value(nullptr);
        }
    }

    void ArgWrapper::WindowsUIColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue)
    {
        if (newValue)
        {
            const auto terminalCoreColor = unbox_value<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>(newValue).Value();
            const Windows::UI::Color winuiColor{
                .A = terminalCoreColor.A,
                .R = terminalCoreColor.R,
                .G = terminalCoreColor.G,
                .B = terminalCoreColor.B
            };
            // only set to the new value if our current value is not the same
            // unfortunately the Value setter does not do this check properly since
            // we create a whole new IReference even for the same underlying color
            if (_Value)
            {
                const auto currentValue = unbox_value<Windows::Foundation::IReference<Windows::UI::Color>>(_Value).Value();
                if (currentValue == winuiColor)
                {
                    return;
                }
            }
            Value(box_value(Windows::Foundation::IReference<Windows::UI::Color>{ winuiColor }));
        }
        else if (_Value)
        {
            Value(nullptr);
        }
    }

    ActionArgsViewModel::ActionArgsViewModel(const Model::ActionAndArgs actionAndArgs) :
        _actionAndArgs{ actionAndArgs }
    {
    }

    void ActionArgsViewModel::Initialize()
    {
        const auto shortcutArgs = _actionAndArgs.Args();
        if (shortcutArgs)
        {
            const auto shortcutArgsNumItems = shortcutArgs.GetArgCount();
            std::vector<Editor::ArgWrapper> argValues;
            for (uint32_t i = 0; i < shortcutArgsNumItems; i++)
            {
                const auto argAtIndex = shortcutArgs.GetArgAt(i);
                const auto argDescription = shortcutArgs.GetArgDescriptionAt(i);
                const auto argName = argDescription.Name;
                const auto argType = argDescription.Type;
                const auto argRequired = argDescription.Required;
                const auto item = make_self<ArgWrapper>(argName, argType, argRequired, argAtIndex);
                item->PropertyChanged([weakThis = get_weak(), i](const IInspectable& sender, const PropertyChangedEventArgs& args) {
                    if (auto weak = weakThis.get())
                    {
                        const auto itemProperty{ args.PropertyName() };
                        if (itemProperty == L"Value")
                        {
                            const auto argWrapper = sender.as<Microsoft::Terminal::Settings::Editor::ArgWrapper>();
                            const auto newValue = argWrapper.Value();
                            weak->_actionAndArgs.Args().SetArgAt(i, newValue);
                            weak->WrapperValueChanged.raise(*weak, nullptr);
                        }
                    }
                });
                item->ColorSchemeRequested([weakThis = get_weak()](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
                    if (auto weak = weakThis.get())
                    {
                        if (wrapper)
                        {
                            weak->PropagateColorSchemeRequested.raise(*weak, wrapper);
                        }
                    }
                });
                item->Initialize();
                argValues.push_back(*item);
            }

            _ArgValues = single_threaded_observable_vector(std::move(argValues));
        }
    }

    bool ActionArgsViewModel::HasArgs() const noexcept
    {
        return _actionAndArgs.Args() != nullptr;
    }

    void ActionArgsViewModel::ReplaceActionAndArgs(Model::ActionAndArgs newActionAndArgs)
    {
        _actionAndArgs = newActionAndArgs;
    }

    KeyChordViewModel::KeyChordViewModel(Control::KeyChord currentKeys)
    {
        CurrentKeys(currentKeys);
    }

    void KeyChordViewModel::CurrentKeys(const Control::KeyChord& newKeys)
    {
        _currentKeys = newKeys;
        KeyChordText(Model::KeyChordSerialization::ToString(_currentKeys));
    }

    Control::KeyChord KeyChordViewModel::CurrentKeys() const noexcept
    {
        return _currentKeys;
    }

    void KeyChordViewModel::ToggleEditMode()
    {
        // toggle edit mode
        IsInEditMode(!_IsInEditMode);
        if (_IsInEditMode)
        {
            // if we're in edit mode,
            // - pre-populate the text box with the current keys
            ProposedKeys(_currentKeys);
        }
    }

    void KeyChordViewModel::AttemptAcceptChanges()
    {
        if (!_currentKeys)
        {
            AddKeyChordRequested.raise(*this, _ProposedKeys);
        }
        else if (_currentKeys.Modifiers() != _ProposedKeys.Modifiers() || _currentKeys.Vkey() != _ProposedKeys.Vkey())
        {
            const auto args{ make_self<ModifyKeyChordEventArgs>(_currentKeys,     // OldKeys
                                                                _ProposedKeys) }; // NewKeys
            ModifyKeyChordRequested.raise(*this, *args);
        }
        else
        {
            // no changes being requested, toggle edit mode
            ToggleEditMode();
        }
    }

    void KeyChordViewModel::CancelChanges()
    {
        ToggleEditMode();
    }

    void KeyChordViewModel::DeleteKeyChord()
    {
        DeleteKeyChordRequested.raise(*this, _currentKeys);
    }

    ActionsViewModel::ActionsViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        _MakeCommandVMsHelper();
    }

    void ActionsViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _Settings = settings;

        // We want to re-initialize our CommandList, but we want to make sure
        // we still have the same CurrentCommand as before (if that command still exists)

        // Store the ID of the current command
        const auto currentCommandID = CurrentCommand() ? CurrentCommand().ID() : hstring{};

        // Re-initialize the command vm list
        _MakeCommandVMsHelper();

        // Re-select the previously selected command if it exists
        if (!currentCommandID.empty())
        {
            const auto it = _CommandList.First();
            while (it.HasCurrent())
            {
                auto cmd = *it;
                if (cmd.ID() == currentCommandID)
                {
                    CurrentCommand(cmd);
                    break;
                }
                it.MoveNext();
            }
            if (!it.HasCurrent())
            {
                // we didn't find the previously selected command
                CurrentCommand(nullptr);
                CurrentPage(ActionsSubPage::Base);
            }
        }
        else
        {
            // didn't have a command,
            // so skip over looking through the command
            CurrentCommand(nullptr);
            CurrentPage(ActionsSubPage::Base);
        }
    }

    void ActionsViewModel::_MakeCommandVMsHelper()
    {
        // Populate AvailableActionAndArgs
        _AvailableActionMap = single_threaded_map<hstring, Model::ActionAndArgs>();
        std::vector<hstring> availableActionAndArgs;
        for (const auto& [name, actionAndArgs] : _Settings.ActionMap().AvailableActions())
        {
            availableActionAndArgs.push_back(name);
            _AvailableActionMap.Insert(name, actionAndArgs);
        }
        std::sort(begin(availableActionAndArgs), end(availableActionAndArgs));
        _AvailableActionAndArgs = single_threaded_observable_vector(std::move(availableActionAndArgs));

        _AvailableActionsAndNamesMap = Model::CascadiaSettings::AvailableShortcutActionsAndNames();
        for (const auto unimplemented : UnimplementedShortcutActions)
        {
            _AvailableActionsAndNamesMap.Remove(unimplemented);
        }

        // Convert the key bindings from our settings into a view model representation
        const auto& keyBindingMap{ _Settings.ActionMap().KeyBindings() };
        std::vector<Editor::KeyBindingViewModel> keyBindingList;
        keyBindingList.reserve(keyBindingMap.Size());
        for (const auto& [keys, cmd] : keyBindingMap)
        {
            // convert the cmd into a KeyBindingViewModel
            auto container{ make_self<KeyBindingViewModel>(keys, cmd.Name(), _AvailableActionAndArgs) };
            _RegisterEvents(container);
            keyBindingList.push_back(*container);
        }

        const auto& allCommands{ _Settings.ActionMap().AllCommands() };
        std::vector<Editor::CommandViewModel> commandList;
        commandList.reserve(allCommands.Size());
        for (const auto& cmd : allCommands)
        {
            if (!UnimplementedShortcutActions.contains(cmd.ActionAndArgs().Action()))
            {
                std::vector<Control::KeyChord> keyChordList;
                for (const auto& keys : _Settings.ActionMap().AllKeyBindingsForAction(cmd.ID()))
                {
                    keyChordList.emplace_back(keys);
                }
                auto cmdVM{ make_self<CommandViewModel>(cmd, keyChordList, *this, _AvailableActionsAndNamesMap) };
                _RegisterCmdVMEvents(cmdVM);
                cmdVM->Initialize();
                commandList.push_back(*cmdVM);
            }
        }

        std::sort(begin(keyBindingList), end(keyBindingList), KeyBindingViewModelComparator{});
        _KeyBindingList = single_threaded_observable_vector(std::move(keyBindingList));
        std::sort(begin(commandList), end(commandList), CommandViewModelComparator{});
        _CommandList = single_threaded_observable_vector(std::move(commandList));
    }

    void ActionsViewModel::OnAutomationPeerAttached()
    {
        _AutomationPeerAttached = true;
        for (const auto& kbdVM : _KeyBindingList)
        {
            // To create a more accessible experience, we want the "edit" buttons to _always_
            // appear when a screen reader is attached. This ensures that the edit buttons are
            // accessible via the UIA tree.
            get_self<KeyBindingViewModel>(kbdVM)->IsAutomationPeerAttached(_AutomationPeerAttached);
        }
    }

    void ActionsViewModel::AddNewKeybinding()
    {
        // Create the new key binding and register all of the event handlers.
        auto kbdVM{ make_self<KeyBindingViewModel>(_AvailableActionAndArgs) };
        _RegisterEvents(kbdVM);
        kbdVM->DeleteNewlyAddedKeyBinding({ this, &ActionsViewModel::_KeyBindingViewModelDeleteNewlyAddedKeyBindingHandler });

        // Manually add the editing background. This needs to be done in Actions not the view model.
        // We also have to do this manually because it hasn't been added to the list yet.
        kbdVM->IsInEditMode(true);
        // Emit an event to let the page know to update the background of this key binding VM
        UpdateBackground.raise(*this, *kbdVM);

        // IMPORTANT: do this _after_ setting IsInEditMode. Otherwise, it'll get deleted immediately
        //              by the PropertyChangedHandler below (where we delete any IsNewlyAdded items)
        kbdVM->IsNewlyAdded(true);
        _KeyBindingList.InsertAt(0, *kbdVM);
        FocusContainer.raise(*this, *kbdVM);
    }

    void ActionsViewModel::AddNewCommand()
    {
        const auto newCmd = Model::Command::NewUserCommand();
        // construct a command using the first shortcut action from our list
        const auto shortcutAction = _AvailableActionsAndNamesMap.First().Current().Key();
        const auto args = CascadiaSettings::GetEmptyArgsForAction(shortcutAction);
        newCmd.ActionAndArgs(Model::ActionAndArgs{ shortcutAction, args });
        _Settings.ActionMap().AddAction(newCmd, nullptr);
        auto cmdVM{ make_self<CommandViewModel>(newCmd, std::vector<Control::KeyChord>{}, *this, _AvailableActionsAndNamesMap) };
        cmdVM->IsNewCommand(true);
        _RegisterCmdVMEvents(cmdVM);
        cmdVM->Initialize();
        _CommandList.Append(*cmdVM);
        CurrentCommand(*cmdVM);
        CurrentPage(ActionsSubPage::Edit);
    }

    void ActionsViewModel::CurrentCommand(const Editor::CommandViewModel& newCommand)
    {
        _CurrentCommand = newCommand;
    }

    Editor::CommandViewModel ActionsViewModel::CurrentCommand()
    {
        return _CurrentCommand;
    }

    void ActionsViewModel::CmdListItemClicked(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs& e)
    {
        if (const auto item = e.ClickedItem())
        {
            CurrentCommand(item.try_as<Editor::CommandViewModel>());
            CurrentPage(ActionsSubPage::Edit);
        }
    }

    void ActionsViewModel::AttemptDeleteKeyChord(const Control::KeyChord& keys)
    {
        // Update the settings model
        if (keys)
        {
            _Settings.ActionMap().DeleteKeyBinding(keys);
        }
    }

    void ActionsViewModel::AttemptAddOrModifyKeyChord(const Editor::KeyChordViewModel& senderVM, winrt::hstring commandID, const Control::KeyChord& newKeys, const Control::KeyChord& oldKeys)
    {
        auto applyChangesToSettingsModel = [=]() {
            // update settings model
            if (oldKeys)
            {
                // if oldKeys is not null, this is a rebinding
                // delete oldKeys and then add newKeys
                _Settings.ActionMap().DeleteKeyBinding(oldKeys);
            }
            if (!Model::KeyChordSerialization::ToString(newKeys).empty())
            {
                _Settings.ActionMap().AddKeyBinding(newKeys, commandID);

                // update view model
                auto senderVMImpl{ get_self<KeyChordViewModel>(senderVM) };
                senderVMImpl->CurrentKeys(newKeys);
            }

            // reset the flyout if it's there
            if (const auto flyout = senderVM.AcceptChangesFlyout())
            {
                flyout.Hide();
                senderVM.AcceptChangesFlyout(nullptr);
            }
            // toggle edit mode
            senderVM.ToggleEditMode();
        };

        const auto& conflictingCmd{ _Settings.ActionMap().GetActionByKeyChord(newKeys) };
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
                // update settings model and view model
                applyChangesToSettingsModel();
            });

            StackPanel flyoutStack{};
            flyoutStack.Children().Append(errorMessageTB);
            flyoutStack.Children().Append(conflictingCommandNameTB);
            flyoutStack.Children().Append(confirmationQuestionTB);
            flyoutStack.Children().Append(acceptBTN);

            Flyout acceptChangesFlyout{};
            acceptChangesFlyout.Content(flyoutStack);
            senderVM.AcceptChangesFlyout(acceptChangesFlyout);
        }
        else
        {
            // update settings model and view model
            applyChangesToSettingsModel();
        }
    }

    void ActionsViewModel::AttemptAddCopiedCommand(const Model::Command& newCommand)
    {
        // The command VM calls this when the user has edited an in-box action
        // newCommand is a copy of the in-box action that was edited, but with OriginTag::User
        // add it to the action map
        _Settings.ActionMap().AddAction(newCommand, nullptr);
    }

    void ActionsViewModel::_KeyBindingViewModelPropertyChangedHandler(const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args)
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
                        // Emit an event to let the page know to move focus to
                        // this VM's container.
                        FocusContainer.raise(*this, senderVM);
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
            }
            else
            {
                // Emit an event to let the page know to move focus to
                // this VM's container.
                FocusContainer.raise(*this, senderVM);
            }

            // Emit an event to let the page know to update the background of this key binding VM
            UpdateBackground.raise(*this, senderVM);
        }
    }

    void ActionsViewModel::_CmdVMPropertyChangedHandler(const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args)
    {
        const auto senderVM{ sender.as<Editor::CommandViewModel>() };
        const auto propertyName{ args.PropertyName() };
        if (propertyName == L"ProposedAction")
        {
        }
    }

    void ActionsViewModel::_KeyBindingViewModelDeleteKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Control::KeyChord& keys)
    {
        // Update the settings model
        _Settings.ActionMap().DeleteKeyBinding(keys);

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
                // Emit an event to let the page know to move focus to
                // this VM's container.
                FocusContainer.raise(*this, winrt::box_value(newFocusedIndex));
            }
        }
    }

    void ActionsViewModel::_KeyBindingViewModelModifyKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const Editor::ModifyKeyBindingEventArgs& args)
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
                    _Settings.ActionMap().RebindKeys(args.OldKeys(), args.NewKeys());
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
                _Settings.ActionMap().RegisterKeyBinding(args.NewKeys(), newAction);

                // update view model
                auto senderVMImpl{ get_self<KeyBindingViewModel>(senderVM) };
                senderVMImpl->CurrentAction(args.NewActionName());
                senderVMImpl->IsNewlyAdded(false);
            }
        };

        // Check for this special case:
        //  we're changing the key chord,
        //  but the new key chord is already in use
        bool conflictFound{ false };
        if (isNewAction || args.OldKeys().Modifiers() != args.NewKeys().Modifiers() || args.OldKeys().Vkey() != args.NewKeys().Vkey())
        {
            const auto& conflictingCmd{ _Settings.ActionMap().GetActionByKeyChord(args.NewKeys()) };
            if (conflictingCmd)
            {
                conflictFound = true;
                // We're about to overwrite another key chord.
                // Display a confirmation dialog.
                TextBlock errorMessageTB{};
                errorMessageTB.Text(RS_(L"Actions_RenameConflictConfirmationMessage"));
                errorMessageTB.TextWrapping(TextWrapping::Wrap);

                const auto conflictingCmdName{ conflictingCmd.Name() };
                TextBlock conflictingCommandNameTB{};
                conflictingCommandNameTB.Text(fmt::format(L"\"{}\"", conflictingCmdName.empty() ? RS_(L"Actions_UnnamedCommandName") : conflictingCmdName));
                conflictingCommandNameTB.FontStyle(Windows::UI::Text::FontStyle::Italic);
                conflictingCommandNameTB.TextWrapping(TextWrapping::Wrap);

                TextBlock confirmationQuestionTB{};
                confirmationQuestionTB.Text(RS_(L"Actions_RenameConflictConfirmationQuestion"));
                confirmationQuestionTB.TextWrapping(TextWrapping::Wrap);

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

                // This should match CustomFlyoutPresenterStyle in CommonResources.xaml!
                // We don't have access to those resources here, so it's easier to just copy them over.
                // This allows the flyout text to wrap
                Style acceptChangesFlyoutStyle{ winrt::xaml_typename<FlyoutPresenter>() };
                Setter horizontalScrollModeStyleSetter{ ScrollViewer::HorizontalScrollModeProperty(), box_value(ScrollMode::Disabled) };
                Setter horizontalScrollBarVisibilityStyleSetter{ ScrollViewer::HorizontalScrollBarVisibilityProperty(), box_value(ScrollBarVisibility::Disabled) };
                acceptChangesFlyoutStyle.Setters().Append(horizontalScrollModeStyleSetter);
                acceptChangesFlyoutStyle.Setters().Append(horizontalScrollBarVisibilityStyleSetter);

                Flyout acceptChangesFlyout{};
                acceptChangesFlyout.FlyoutPresenterStyle(acceptChangesFlyoutStyle);
                acceptChangesFlyout.Content(flyoutStack);
                senderVM.AcceptChangesFlyout(acceptChangesFlyout);
            }
        }

        // if there was a conflict, the flyout we created will handle whether changes need to be propagated
        // otherwise, go ahead and apply the changes
        if (!conflictFound)
        {
            // update settings model and view model
            applyChangesToSettingsModel();

            // We NEED to toggle the edit mode here,
            // so that if nothing changed, we still exit
            // edit mode.
            senderVM.ToggleEditMode();
        }
    }

    void ActionsViewModel::_KeyBindingViewModelDeleteNewlyAddedKeyBindingHandler(const Editor::KeyBindingViewModel& senderVM, const IInspectable& /*args*/)
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

    void ActionsViewModel::_CmdVMEditRequestedHandler(const Editor::CommandViewModel& senderVM, const IInspectable& /*args*/)
    {
        CurrentCommand(senderVM);
        CurrentPage(ActionsSubPage::Edit);
    }

    void ActionsViewModel::_CmdVMDeleteRequestedHandler(const Editor::CommandViewModel& senderVM, const IInspectable& /*args*/)
    {
        for (uint32_t i = 0; i < _CommandList.Size(); i++)
        {
            if (_CommandList.GetAt(i) == senderVM)
            {
                CommandList().RemoveAt(i);
                break;
            }
        }
        _Settings.ActionMap().DeleteUserCommand(senderVM.ID());
        CurrentCommand(nullptr);
        CurrentPage(ActionsSubPage::Base);
    }

    void ActionsViewModel::_CmdVMPropagateColorSchemeRequestedHandler(const IInspectable& /*senderVM*/, const Editor::ArgWrapper& wrapper)
    {
        if (wrapper)
        {
            const auto schemes = _Settings.GlobalSettings().ColorSchemes();
            const auto defaultAppearanceSchemeName = _Settings.ProfileDefaults().DefaultAppearance().LightColorSchemeName();
            for (const auto [name, scheme] : schemes)
            {
                if (name == defaultAppearanceSchemeName)
                {
                    const auto schemeVM = winrt::make<ColorSchemeViewModel>(scheme, nullptr, _Settings);
                    wrapper.DefaultColorScheme(schemeVM);
                }
            }
        }
    }

    // Method Description:
    // - performs a search on KeyBindingList by key chord.
    // Arguments:
    // - keys - the associated key chord of the command we're looking for
    // Return Value:
    // - the index of the view model referencing the command. If the command doesn't exist, nullopt
    std::optional<uint32_t> ActionsViewModel::_GetContainerIndexByKeyChord(const Control::KeyChord& keys)
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

    void ActionsViewModel::_RegisterEvents(com_ptr<KeyBindingViewModel>& kbdVM)
    {
        kbdVM->PropertyChanged({ this, &ActionsViewModel::_KeyBindingViewModelPropertyChangedHandler });
        kbdVM->DeleteKeyBindingRequested({ this, &ActionsViewModel::_KeyBindingViewModelDeleteKeyBindingHandler });
        kbdVM->ModifyKeyBindingRequested({ this, &ActionsViewModel::_KeyBindingViewModelModifyKeyBindingHandler });
        kbdVM->IsAutomationPeerAttached(_AutomationPeerAttached);
    }

    void ActionsViewModel::_RegisterCmdVMEvents(com_ptr<CommandViewModel>& cmdVM)
    {
        cmdVM->EditRequested({ this, &ActionsViewModel::_CmdVMEditRequestedHandler });
        cmdVM->DeleteRequested({ this, &ActionsViewModel::_CmdVMDeleteRequestedHandler });
        cmdVM->PropertyChanged({ this, &ActionsViewModel::_CmdVMPropertyChangedHandler });
        cmdVM->PropagateColorSchemeRequested({ this, &ActionsViewModel::_CmdVMPropagateColorSchemeRequestedHandler });
    }
}
