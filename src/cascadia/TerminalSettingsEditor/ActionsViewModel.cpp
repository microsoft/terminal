// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionsViewModel.h"
#include "ActionsViewModel.g.cpp"
#include "CommandViewModel.g.cpp"
#include "ArgWrapper.g.cpp"
#include "ActionArgsViewModel.g.cpp"
#include "KeyChordViewModel.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"
#include "EnumEntry.h"
#include "ColorSchemeViewModel.h"
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

// TODO: GH 19056
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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static constexpr std::wstring_view ActionsPageId{ L"page.actions" };

    CommandViewModel::CommandViewModel(const Command& cmd, std::vector<Control::KeyChord> keyChordList, const Editor::ActionsViewModel& actionsPageVM, Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> availableActionsAndNamesMap, Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> nameToActionMap) :
        _command{ cmd },
        _keyChordList{ std::move(keyChordList) },
        _actionsPageVM{ actionsPageVM },
        _availableActionsAndNamesMap{ availableActionsAndNamesMap },
        _nameToActionMap{ nameToActionMap }
    {
    }

    void CommandViewModel::Initialize()
    {
        const auto actionsPageVM{ _actionsPageVM.get() };
        if (!actionsPageVM)
        {
            // The parent page is gone, just return early
            return;
        }
        std::vector<Editor::KeyChordViewModel> keyChordVMs;
        for (const auto keys : _keyChordList)
        {
            auto kcVM{ make<KeyChordViewModel>(keys) };
            _RegisterKeyChordVMEvents(kcVM);
            keyChordVMs.push_back(kcVM);
        }
        _KeyChordList = single_threaded_observable_vector(std::move(keyChordVMs));

        std::vector<hstring> shortcutActions;
        for (const auto [action, name] : _availableActionsAndNamesMap)
        {
            shortcutActions.emplace_back(name);
        }
        std::sort(shortcutActions.begin(), shortcutActions.end());
        _AvailableShortcutActions = single_threaded_observable_vector(std::move(shortcutActions));

        const auto shortcutActionString = _availableActionsAndNamesMap.Lookup(_command.ActionAndArgs().Action());
        ProposedShortcutActionName(winrt::box_value(shortcutActionString));
        _CreateAndInitializeActionArgsVMHelper();

        // Add a property changed handler to our own property changed event.
        // This allows us to create a new ActionArgsVM when the shortcut action changes
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            if (const auto actionsPageVM{ _actionsPageVM.get() })
            {
                const auto viewModelProperty{ args.PropertyName() };
                if (viewModelProperty == L"ProposedShortcutActionName")
                {
                    const auto actionString = unbox_value<hstring>(ProposedShortcutActionName());
                    const auto actionEnum = _nameToActionMap.Lookup(actionString);
                    const auto emptyArgs = ActionArgFactory::GetEmptyArgsForAction(actionEnum);
                    // TODO: GH 19056
                    // probably need some better default values for empty args and/or validation
                    // eg. for sendInput, where "input" is a required argument, "input" gets set to an empty string which does not satisfy the requirement
                    // i.e. if the user hits "save" immediately after switching to sendInput as the action (without adding something to the input field), they'll get an error
                    // there are some other cases as well
                    Model::ActionAndArgs newActionAndArgs{ actionEnum, emptyArgs };
                    _command.ActionAndArgs(newActionAndArgs);
                    if (_IsNewCommand)
                    {
                        actionsPageVM.RegenerateCommandID(_command);
                    }
                    else if (!IsUserAction())
                    {
                        _ReplaceCommandWithUserCopy(true);
                        return;
                    }
                    _CreateAndInitializeActionArgsVMHelper();
                }
            }
        });
    }

    winrt::hstring CommandViewModel::DisplayName()
    {
        if (_cachedDisplayName.empty())
        {
            _cachedDisplayName = _command.Name();
        }
        return _cachedDisplayName;
    }

    winrt::hstring CommandViewModel::Name()
    {
        return _command.HasName() ? _command.Name() : L"";
    }

    void CommandViewModel::Name(const winrt::hstring& newName)
    {
        _command.Name(newName);
        if (newName.empty())
        {
            // if the name was cleared, refresh the DisplayName
            _NotifyChanges(L"DisplayName", L"DisplayNameAndKeyChordAutomationPropName");
        }
        _cachedDisplayName.clear();
    }

    winrt::hstring CommandViewModel::DisplayNameAndKeyChordAutomationPropName()
    {
        return DisplayName() + L", " + FirstKeyChordText();
    }

    winrt::hstring CommandViewModel::FirstKeyChordText()
    {
        if (_KeyChordList.Size() != 0)
        {
            return _KeyChordList.GetAt(0).KeyChordText();
        }
        return L"";
    }

    winrt::hstring CommandViewModel::ID()
    {
        return _command.ID();
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
        KeyChordList().Append(*kbdVM);
    }

    winrt::hstring CommandViewModel::ActionNameTextBoxAutomationPropName()
    {
        return RS_(L"Actions_Name/Text");
    }

    winrt::hstring CommandViewModel::ShortcutActionComboBoxAutomationPropName()
    {
        return RS_(L"Actions_ShortcutAction/Text");
    }

    winrt::hstring CommandViewModel::AdditionalArgumentsControlAutomationPropName()
    {
        return RS_(L"Actions_Arguments/Text");
    }

    void CommandViewModel::_RegisterKeyChordVMEvents(Editor::KeyChordViewModel kcVM)
    {
        const auto id = ID();
        kcVM.AddKeyChordRequested([actionsPageVMWeakRef = _actionsPageVM, id](const Editor::KeyChordViewModel& sender, const Control::KeyChord& keys) {
            if (const auto actionsPageVM{ actionsPageVMWeakRef.get() })
            {
                actionsPageVM.AttemptAddOrModifyKeyChord(sender, id, keys, nullptr);
            }
        });
        kcVM.ModifyKeyChordRequested([actionsPageVMWeakRef = _actionsPageVM, id](const Editor::KeyChordViewModel& sender, const Editor::ModifyKeyChordEventArgs& args) {
            if (const auto actionsPageVM{ actionsPageVMWeakRef.get() })
            {
                actionsPageVM.AttemptAddOrModifyKeyChord(sender, id, args.NewKeys(), args.OldKeys());
            }
        });
        kcVM.DeleteKeyChordRequested([&, actionsPageVMWeakRef = _actionsPageVM](const Editor::KeyChordViewModel& sender, const Control::KeyChord& args) {
            if (const auto actionsPageVM{ actionsPageVMWeakRef.get() })
            {
                std::erase_if(_keyChordList,
                              [&](const Control::KeyChord& kc) { return kc == args; });
                for (uint32_t i = 0; i < _KeyChordList.Size(); i++)
                {
                    if (_KeyChordList.GetAt(i) == sender)
                    {
                        KeyChordList().RemoveAt(i);
                        break;
                    }
                }
                actionsPageVM.DeleteKeyChord(args);
            }
        });
        kcVM.PropertyChanged([weakThis{ get_weak() }](const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
            if (const auto self{ weakThis.get() })
            {
                const auto senderVM{ sender.as<Editor::KeyChordViewModel>() };
                const auto propertyName{ args.PropertyName() };
                if (propertyName == L"IsInEditMode")
                {
                    if (!senderVM.IsInEditMode())
                    {
                        self->FocusContainer.raise(*self, senderVM);
                    }
                }
            }
        });
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
        actionArgsVM.PropagateColorSchemeNamesRequested([weakThis = get_weak()](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
            if (auto weak = weakThis.get())
            {
                if (wrapper)
                {
                    weak->PropagateColorSchemeNamesRequested.raise(*weak, wrapper);
                }
            }
        });
        actionArgsVM.PropagateWindowRootRequested([weakThis = get_weak()](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
            if (auto weak = weakThis.get())
            {
                if (wrapper)
                {
                    weak->PropagateWindowRootRequested.raise(*weak, wrapper);
                }
            }
        });
        actionArgsVM.WrapperValueChanged([weakThis = get_weak()](const IInspectable& /*sender*/, const IInspectable& /*args*/) {
            if (auto weak = weakThis.get())
            {
                // for new commands, make sure we generate a new ID every time any arg value changes
                if (weak->_IsNewCommand)
                {
                    if (const auto actionsPageVM{ weak->_actionsPageVM.get() })
                    {
                        actionsPageVM.RegenerateCommandID(weak->_command);
                    }
                }
                else if (!weak->IsUserAction())
                {
                    weak->_ReplaceCommandWithUserCopy(false);
                }
                weak->_NotifyChanges(L"DisplayName");
            }
        });
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
            actionsPageVM.AddCopiedCommand(_command);
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

    ArgWrapper::ArgWrapper(const Model::ArgDescriptor& descriptor, const Windows::Foundation::IInspectable& value) :
        _descriptor{ descriptor }
    {
        Value(value);
    }

    void ArgWrapper::Initialize()
    {
        if (_descriptor.Type == L"Model::ResizeDirection")
        {
            _InitializeEnumListAndValue<Model::ResizeDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::ResizeDirection().GetView(),
                L"Actions_ResizeDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"Model::FocusDirection")
        {
            _InitializeEnumListAndValue<Model::FocusDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::FocusDirection().GetView(),
                L"Actions_FocusDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"SettingsTarget")
        {
            _InitializeEnumListAndValue<Model::SettingsTarget>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::SettingsTarget().GetView(),
                L"Actions_SettingsTarget",
                L"Content");
        }
        else if (_descriptor.Type == L"MoveTabDirection")
        {
            _InitializeEnumListAndValue<Model::MoveTabDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::MoveTabDirection().GetView(),
                L"Actions_MoveTabDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"Microsoft::Terminal::Control::ScrollToMarkDirection")
        {
            _InitializeEnumListAndValue<Control::ScrollToMarkDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::ScrollToMarkDirection().GetView(),
                L"Actions_ScrollToMarkDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"CommandPaletteLaunchMode")
        {
            _InitializeEnumListAndValue<Model::CommandPaletteLaunchMode>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::CommandPaletteLaunchMode().GetView(),
                L"Actions_CommandPaletteLaunchMode",
                L"Content");
        }
        else if (_descriptor.Type == L"SuggestionsSource")
        {
            _InitializeFlagListAndValue<Model::SuggestionsSource>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::SuggestionsSource().GetView(),
                L"Actions_SuggestionsSource",
                L"Content");
        }
        else if (_descriptor.Type == L"FindMatchDirection")
        {
            _InitializeEnumListAndValue<Model::FindMatchDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::FindMatchDirection().GetView(),
                L"Actions_FindMatchDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"Model::DesktopBehavior")
        {
            _InitializeEnumListAndValue<Model::DesktopBehavior>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::DesktopBehavior().GetView(),
                L"Actions_DesktopBehavior",
                L"Content");
        }
        else if (_descriptor.Type == L"Model::MonitorBehavior")
        {
            _InitializeEnumListAndValue<Model::MonitorBehavior>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::MonitorBehavior().GetView(),
                L"Actions_MonitorBehavior",
                L"Content");
        }
        else if (_descriptor.Type == L"winrt::Microsoft::Terminal::Control::ClearBufferType")
        {
            _InitializeEnumListAndValue<Control::ClearBufferType>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::ClearBufferType().GetView(),
                L"Actions_ClearBufferType",
                L"Content");
        }
        else if (_descriptor.Type == L"SelectOutputDirection")
        {
            _InitializeEnumListAndValue<Model::SelectOutputDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::SelectOutputDirection().GetView(),
                L"Actions_SelectOutputDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"Model::SplitDirection")
        {
            _InitializeEnumListAndValue<Model::SplitDirection>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::SplitDirection().GetView(),
                L"Actions_SplitDirection",
                L"Content");
        }
        else if (_descriptor.Type == L"SplitType")
        {
            _InitializeEnumListAndValue<Model::SplitType>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::SplitType().GetView(),
                L"Actions_SplitType",
                L"Content");
        }
        else if (_descriptor.Type == L"Windows::Foundation::IReference<TabSwitcherMode>")
        {
            _InitializeNullableEnumListAndValue<Model::TabSwitcherMode>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::TabSwitcherMode().GetView(),
                L"Actions_TabSwitcherMode",
                L"Content");
        }
        else if (_descriptor.Type == L"Windows::Foundation::IReference<Control::CopyFormat>")
        {
            _InitializeNullableFlagListAndValue<Control::CopyFormat>(
                winrt::Microsoft::Terminal::Settings::Model::EnumMappings::CopyFormat().GetView(),
                L"Actions_CopyFormat",
                L"Content");
        }
        else if (_descriptor.Type == L"Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>" ||
                 _descriptor.Type == L"Windows::Foundation::IReference<Windows::UI::Color>")
        {
            ColorSchemeRequested.raise(*this, *this);
        }
        else if (_descriptor.TypeHint == Model::ArgTypeHint::ColorScheme)
        {
            // special case of string, emit an event letting the actionsVM know we need the list of color scheme names
            ColorSchemeNamesRequested.raise(*this, *this);

            // even though the arg type is technically a string, we want an enum list for color schemes specifically
            std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> namesList;
            const auto currentSchemeName = unbox_value<winrt::hstring>(_Value);
            auto nullEntry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(RS_(L"Actions_NullEnumValue"), nullptr, -1);
            if (currentSchemeName.empty())
            {
                _EnumValue = nullEntry;
            }
            for (const auto colorSchemeName : _ColorSchemeNamesList)
            {
                // eventually we will want to use localized names for the enum entries, for now just use what the settings model gives us
                auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(colorSchemeName, winrt::box_value(colorSchemeName));
                namesList.emplace_back(entry);
                if (currentSchemeName == colorSchemeName)
                {
                    _EnumValue = entry;
                }
            }
            namesList.emplace_back(nullEntry);
            _EnumList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(namesList));
            _NotifyChanges(L"EnumList", L"EnumValue");
        }
    }

    safe_void_coroutine ArgWrapper::BrowseForFile_Click(const IInspectable&, const RoutedEventArgs&)
    {
        WindowRootRequested.raise(*this, *this);
        auto lifetime = get_strong();

        static constexpr winrt::guid clientGuidFiles{ 0xbd00ae34, 0x839b, 0x43f6, { 0x8b, 0x94, 0x12, 0x37, 0x1a, 0xfe, 0xea, 0xb5 } };
        const auto parentHwnd{ reinterpret_cast<HWND>(_WindowRoot.GetHostingWindow()) };
        auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidFiles));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal
        });

        if (!path.empty())
        {
            StringBindBack(path);
        }
    }

    safe_void_coroutine ArgWrapper::BrowseForFolder_Click(const IInspectable&, const RoutedEventArgs&)
    {
        WindowRootRequested.raise(*this, *this);
        auto lifetime = get_strong();

        static constexpr winrt::guid clientGuidFolders{ 0xa611027, 0x42be, 0x4665, { 0xaf, 0xf1, 0x3f, 0x22, 0x26, 0xe9, 0xf7, 0x4d } };
        ;
        const auto parentHwnd{ reinterpret_cast<HWND>(_WindowRoot.GetHostingWindow()) };
        auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidFolders));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal

            DWORD flags{};
            THROW_IF_FAILED(dialog->GetOptions(&flags));
            THROW_IF_FAILED(dialog->SetOptions(flags | FOS_PICKFOLDERS)); // folders only
        });

        if (!path.empty())
        {
            StringBindBack(path);
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
        else if (_Value)
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
        else if (_Value)
        {
            Value(nullptr);
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

    template<typename EnumType>
    void ArgWrapper::_InitializeEnumListAndValue(
        const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
        const winrt::hstring& resourceSectionAndType,
        const winrt::hstring& resourceProperty)
    {
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> enumList;
        std::unordered_set<EnumType> addedEnums;
        EnumType unboxedValue{};

        if (_Value)
        {
            unboxedValue = winrt::unbox_value<EnumType>(_Value);
        }

        for (const auto& [enumKey, enumValue] : mappings)
        {
            if (addedEnums.emplace(enumValue).second)
            {
                winrt::hstring enumName = LocalizedNameForEnumName(resourceSectionAndType, enumKey, resourceProperty);
                auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(
                    enumName, winrt::box_value<EnumType>(enumValue), static_cast<int32_t>(enumValue));
                enumList.emplace_back(entry);
                if (_Value && unboxedValue == enumValue)
                {
                    _EnumValue = entry;
                }
            }
        }
        std::sort(enumList.begin(), enumList.end(), winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntryReverseComparator<EnumType>());
        _EnumList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(enumList));
        if (!_EnumValue)
        {
            _EnumValue = _EnumList.GetAt(0);
        }
    }

    template<typename EnumType>
    void ArgWrapper::_InitializeNullableEnumListAndValue(
        const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
        const winrt::hstring& resourceSectionAndType,
        const winrt::hstring& resourceProperty)
    {
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> enumList;
        std::unordered_set<EnumType> addedEnums;
        EnumType unboxedValue{};

        auto nullEntry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(
            RS_(L"Actions_NullEnumValue"), nullptr, -1);

        if (_Value)
        {
            unboxedValue = winrt::unbox_value<EnumType>(_Value);
        }
        else
        {
            _EnumValue = nullEntry;
        }

        for (const auto& [enumKey, enumValue] : mappings)
        {
            if (addedEnums.emplace(enumValue).second)
            {
                winrt::hstring enumName = LocalizedNameForEnumName(resourceSectionAndType, enumKey, resourceProperty);
                auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(
                    enumName, winrt::box_value<EnumType>(enumValue), static_cast<int32_t>(enumValue));
                enumList.emplace_back(entry);
                if (_Value && unboxedValue == enumValue)
                {
                    _EnumValue = entry;
                }
            }
        }
        std::sort(enumList.begin(), enumList.end(), winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntryReverseComparator<EnumType>());
        enumList.emplace_back(nullEntry);
        _EnumList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(enumList));
    }

    template<typename EnumType>
    void ArgWrapper::_InitializeFlagListAndValue(
        const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
        const winrt::hstring& resourceSectionAndType,
        const winrt::hstring& resourceProperty)
    {
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry> flagList;
        std::unordered_set<EnumType> addedEnums;
        EnumType unboxedValue{ 0 };

        if (_Value)
        {
            unboxedValue = winrt::unbox_value<EnumType>(_Value);
        }

        for (const auto& [flagKey, flagValue] : mappings)
        {
            if (flagKey != L"all" && flagKey != L"none" && addedEnums.emplace(flagValue).second)
            {
                winrt::hstring flagName = LocalizedNameForEnumName(resourceSectionAndType, flagKey, resourceProperty);
                bool isSet = WI_IsAnyFlagSet(unboxedValue, flagValue);
                auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(
                    flagName, winrt::box_value<EnumType>(flagValue), isSet, static_cast<int32_t>(flagValue));
                entry.PropertyChanged([this, flagValue](const winrt::Windows::Foundation::IInspectable& sender,
                                                        const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
                    const auto itemProperty = args.PropertyName();
                    if (itemProperty == L"IsSet")
                    {
                        auto flagWrapper = sender.as<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>();
                        auto unboxed = winrt::unbox_value<EnumType>(_Value);
                        if (flagWrapper.IsSet())
                        {
                            WI_SetAllFlags(unboxed, flagValue);
                        }
                        else
                        {
                            WI_ClearAllFlags(unboxed, flagValue);
                        }
                        Value(winrt::box_value(unboxed));
                    }
                });
                flagList.emplace_back(entry);
            }
        }
        std::sort(flagList.begin(), flagList.end(), winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntryReverseComparator<EnumType>());
        _FlagList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>(std::move(flagList));
    }

    template<typename EnumType>
    void ArgWrapper::_InitializeNullableFlagListAndValue(
        const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
        const winrt::hstring& resourceSectionAndType,
        const winrt::hstring& resourceProperty)
    {
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry> flagList;
        std::unordered_set<EnumType> addedEnums;
        EnumType unboxedValue{ 0 };

        auto nullEntry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(
            RS_(L"Actions_NullEnumValue"), nullptr, true, -1);

        if (_Value)
        {
            if (auto unboxedRef = winrt::unbox_value<winrt::Windows::Foundation::IReference<EnumType>>(_Value))
            {
                unboxedValue = unboxedRef.Value();
                nullEntry.IsSet(false);
            }
        }

        for (const auto& [flagKey, flagValue] : mappings)
        {
            if (flagKey != L"all" && flagKey != L"none" && addedEnums.emplace(flagValue).second)
            {
                winrt::hstring flagName = LocalizedNameForEnumName(resourceSectionAndType, flagKey, resourceProperty);
                bool isSet = WI_IsAnyFlagSet(unboxedValue, flagValue);
                auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntry>(
                    flagName, winrt::box_value<EnumType>(flagValue), isSet, static_cast<int32_t>(flagValue));
                entry.PropertyChanged([this, flagValue, nullEntry](const winrt::Windows::Foundation::IInspectable& sender,
                                                                   const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
                    const auto itemProperty = args.PropertyName();
                    if (itemProperty == L"IsSet")
                    {
                        auto flagWrapper = sender.as<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>();
                        EnumType localUnboxed{ 0 };
                        auto unboxedRef = winrt::unbox_value<winrt::Windows::Foundation::IReference<EnumType>>(_Value);
                        if (unboxedRef)
                        {
                            localUnboxed = unboxedRef.Value();
                        }

                        if (flagWrapper.IsSet())
                        {
                            nullEntry.IsSet(false);
                            WI_SetAllFlags(localUnboxed, flagValue);
                        }
                        else
                        {
                            WI_ClearAllFlags(localUnboxed, flagValue);
                        }
                        Value(winrt::box_value(winrt::Windows::Foundation::IReference<EnumType>(localUnboxed)));
                    }
                });
                flagList.emplace_back(entry);
            }
        }
        std::sort(flagList.begin(), flagList.end(), winrt::Microsoft::Terminal::Settings::Editor::implementation::FlagEntryReverseComparator<EnumType>());
        // nullEntry handler that toggles/clears other flags
        nullEntry.PropertyChanged([this](const winrt::Windows::Foundation::IInspectable& sender,
                                         const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
            const auto itemProperty = args.PropertyName();
            if (itemProperty == L"IsSet")
            {
                auto flagWrapper = sender.as<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>();
                if (flagWrapper.IsSet())
                {
                    for (const auto& flagEntry : _FlagList)
                    {
                        if (flagEntry.FlagName() != RS_(L"Actions_NullEnumValue"))
                        {
                            flagEntry.IsSet(false);
                        }
                    }
                    Value(winrt::box_value(winrt::Windows::Foundation::IReference<EnumType>(nullptr)));
                }
                else
                {
                    Value(winrt::box_value(winrt::Windows::Foundation::IReference<EnumType>(EnumType{ 0 })));
                }
            }
        });
        flagList.emplace_back(nullEntry);
        _FlagList = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::FlagEntry>(std::move(flagList));
    }

    ActionArgsViewModel::ActionArgsViewModel(const Model::ActionAndArgs actionAndArgs) :
        _actionAndArgs{ actionAndArgs }
    {
    }

    void ActionArgsViewModel::Initialize()
    {
        const auto shortcutArgs = _actionAndArgs.Args().as<Model::IActionArgsDescriptorAccess>();
        if (shortcutArgs)
        {
            const auto shortcutArgsDescriptors = shortcutArgs.GetArgDescriptors();
            std::vector<Editor::ArgWrapper> argValues;
            uint32_t i = 0;
            for (const auto argDescription : shortcutArgsDescriptors)
            {
                const auto argAtIndex = shortcutArgs.GetArgAt(i);
                const auto item = make_self<ArgWrapper>(argDescription, argAtIndex);
                item->PropertyChanged([this, i](const IInspectable& sender, const PropertyChangedEventArgs& args) {
                    const auto itemProperty{ args.PropertyName() };
                    if (itemProperty == L"Value")
                    {
                        const auto argWrapper = sender.as<Microsoft::Terminal::Settings::Editor::ArgWrapper>();
                        const auto newValue = argWrapper.Value();
                        _actionAndArgs.Args().as<Model::IActionArgsDescriptorAccess>().SetArgAt(i, newValue);
                        WrapperValueChanged.raise(*this, nullptr);
                    }
                });
                item->ColorSchemeRequested([this](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
                    if (wrapper)
                    {
                        PropagateColorSchemeRequested.raise(*this, wrapper);
                    }
                });
                item->ColorSchemeNamesRequested([this](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
                    if (wrapper)
                    {
                        PropagateColorSchemeNamesRequested.raise(*this, wrapper);
                    }
                });
                item->WindowRootRequested([this](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
                    if (wrapper)
                    {
                        PropagateWindowRootRequested.raise(*this, wrapper);
                    }
                });
                item->Initialize();
                argValues.push_back(*item);
                i++;
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
            // if we're in edit mode, populate the text box with the current keys
            ProposedKeys(_currentKeys);
        }
    }

    void KeyChordViewModel::AcceptChanges()
    {
        if (!_currentKeys)
        {
            AddKeyChordRequested.raise(*this, _ProposedKeys);
        }
        else if (_currentKeys.Modifiers() != _ProposedKeys.Modifiers() || _currentKeys.Vkey() != _ProposedKeys.Vkey())
        {
            const auto args{ make_self<ModifyKeyChordEventArgs>(_currentKeys, // OldKeys
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

    hstring KeyChordViewModel::CancelButtonName() const noexcept { return RS_(L"Actions_CancelButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }
    hstring KeyChordViewModel::AcceptButtonName() const noexcept { return RS_(L"Actions_AcceptButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }
    hstring KeyChordViewModel::DeleteButtonName() const noexcept { return RS_(L"Actions_DeleteButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"); }

    ActionsViewModel::ActionsViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        // Initialize the action->name and name->action maps before initializing the CommandVMs, they're going to need the maps
        _AvailableActionsAndNamesMap = Model::ActionArgFactory::AvailableShortcutActionsAndNames();
        for (const auto unimplemented : UnimplementedShortcutActions)
        {
            _AvailableActionsAndNamesMap.Remove(unimplemented);
        }
        std::unordered_map<winrt::hstring, Model::ShortcutAction> actionNames;
        for (const auto [action, name] : _AvailableActionsAndNamesMap)
        {
            actionNames.emplace(name, action);
        }
        _NameToActionMap = winrt::single_threaded_map(std::move(actionNames));

        _MakeCommandVMsHelper();
    }

    Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> ActionsViewModel::AvailableShortcutActionsAndNames()
    {
        return _AvailableActionsAndNamesMap;
    }

    Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> ActionsViewModel::NameToActionMap()
    {
        return _NameToActionMap;
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
        bool found{ false };
        if (!currentCommandID.empty())
        {
            const auto it = _CommandList.First();
            while (it.HasCurrent())
            {
                auto cmd = *it;
                if (cmd.ID() == currentCommandID)
                {
                    CurrentCommand(cmd);
                    found = true;
                    break;
                }
                it.MoveNext();
            }
        }
        if (!found)
        {
            // didn't have a command,
            // so skip over looking through the command
            CurrentCommand(nullptr);
            CurrentPage(ActionsSubPage::Base);
        }
    }

    void ActionsViewModel::MarkAsVisited()
    {
        Model::ApplicationState::SharedInstance().DismissBadge(ActionsPageId);
        _NotifyChanges(L"DisplayBadge");
    }

    bool ActionsViewModel::DisplayBadge() const noexcept
    {
        return !Model::ApplicationState::SharedInstance().BadgeDismissed(ActionsPageId);
    }

    void ActionsViewModel::_MakeCommandVMsHelper()
    {
        const auto& allCommands{ _Settings.ActionMap().AllCommands() };
        std::vector<Editor::CommandViewModel> commandList;
        commandList.reserve(allCommands.Size());
        for (const auto& cmd : allCommands)
        {
            if (!UnimplementedShortcutActions.contains(cmd.ActionAndArgs().Action()))
            {
                std::vector<Control::KeyChord> keyChordList = wil::to_vector(_Settings.ActionMap().AllKeyBindingsForAction(cmd.ID()));
                auto cmdVM{ make_self<CommandViewModel>(cmd, std::move(keyChordList), *this, _AvailableActionsAndNamesMap, _NameToActionMap) };
                _RegisterCmdVMEvents(cmdVM);
                cmdVM->Initialize();
                commandList.push_back(*cmdVM);
            }
        }

        std::sort(commandList.begin(), commandList.end(), [](const Editor::CommandViewModel& lhs, const Editor::CommandViewModel& rhs) {
            return lhs.DisplayName() < rhs.DisplayName();
        });
        _CommandList = single_threaded_observable_vector(std::move(commandList));
    }

    void ActionsViewModel::AddNewCommand()
    {
        const auto newCmd = Model::Command::NewUserCommand();
        // construct a command using the first shortcut action from our list
        const auto shortcutAction = _AvailableActionsAndNamesMap.First().Current().Key();
        const auto args = ActionArgFactory::GetEmptyArgsForAction(shortcutAction);
        newCmd.ActionAndArgs(Model::ActionAndArgs{ shortcutAction, args });
        _Settings.ActionMap().AddAction(newCmd, nullptr);
        auto cmdVM{ make_self<CommandViewModel>(newCmd, std::vector<Control::KeyChord>{}, *this, _AvailableActionsAndNamesMap, _NameToActionMap) };
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

    void ActionsViewModel::DeleteKeyChord(const Control::KeyChord& keys)
    {
        // Update the settings model
        assert(keys);
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

    void ActionsViewModel::AddCopiedCommand(const Model::Command& newCommand)
    {
        // The command VM calls this when the user has edited an in-box action
        // newCommand is a copy of the in-box action that was edited, but with OriginTag::User
        // add it to the action map
        _Settings.ActionMap().AddAction(newCommand, nullptr);
    }

    void ActionsViewModel::RegenerateCommandID(const Model::Command& command)
    {
        _Settings.UpdateCommandID(command, {});
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
                    break;
                }
            }
        }
    }

    void ActionsViewModel::_CmdVMPropagateColorSchemeNamesRequestedHandler(const IInspectable& /*senderVM*/, const Editor::ArgWrapper& wrapper)
    {
        if (wrapper)
        {
            std::vector<winrt::hstring> namesList;
            const auto schemes = _Settings.GlobalSettings().ColorSchemes();
            for (const auto [name, _] : schemes)
            {
                namesList.emplace_back(name);
            }
            wrapper.ColorSchemeNamesList(winrt::single_threaded_vector<winrt::hstring>(std::move(namesList)));
        }
    }

    void ActionsViewModel::_RegisterCmdVMEvents(com_ptr<CommandViewModel>& cmdVM)
    {
        cmdVM->EditRequested({ this, &ActionsViewModel::_CmdVMEditRequestedHandler });
        cmdVM->DeleteRequested({ this, &ActionsViewModel::_CmdVMDeleteRequestedHandler });
        cmdVM->PropagateColorSchemeRequested({ this, &ActionsViewModel::_CmdVMPropagateColorSchemeRequestedHandler });
        cmdVM->PropagateColorSchemeNamesRequested({ this, &ActionsViewModel::_CmdVMPropagateColorSchemeNamesRequestedHandler });
    }
}
