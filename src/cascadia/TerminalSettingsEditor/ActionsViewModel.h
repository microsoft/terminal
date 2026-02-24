/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ActionsViewModel.h

Abstract:
- This contains the view models for everything related to the Actions pages (Actions.xaml and EditAction.xaml)
- ActionsViewModel:
    - Contains the "current page" enum, which dictates whether we're in the top-level Actions page or the EditAction page
    - Contains the full command list, and keeps track of the "current command" that is being edited
        - These are in the form of CommandViewModel(s)
    - Handles modification to the list of commands, i.e. addition/deletion
    - Listens to each CommandViewModel for key chord events for addition/modification/deletion of keychords
- CommandViewModel:
    - Constructed with a Model::Command object
    - View model for each specific command item
    - Contains higher-level detail about the command itself such as name, whether it is a user command, and the shortcut action type
    - Contains an ActionArgsViewModel, which it creates according to the shortcut action type
        - Recreates the ActionArgsViewModel whenever the shortcut action type changes
    - Contains the full keybinding list, in the form of KeyChordViewModel(s)
- ActionArgsViewModel:
    - Constructed with a Model::ActionAndArgs object
    - Contains a vector of ArgWrapper(s), one ArgWrapper for each arg
    - Listens and propagates changes to the ArgWrappers
- ArgWrapper:
    - Wrapper for each argument
    - Handles binding and bind back logic for the presentation and modification of the argument via the UI
        - Has separate binding and bind back logic depending on the type of the argument
- KeyChordViewModel:
    - Constructed with a Control::KeyChord object
    - Handles binding and bind back logic for the presentation and modification of a keybinding via the UI

--*/

#pragma once

#include "ActionsViewModel.g.h"
#include "CommandViewModel.g.h"
#include "ArgWrapper.g.h"
#include "ActionArgsViewModel.g.h"
#include "KeyChordViewModel.g.h"
#include "ModifyKeyChordEventArgs.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ModifyKeyChordEventArgs : ModifyKeyChordEventArgsT<ModifyKeyChordEventArgs>
    {
    public:
        ModifyKeyChordEventArgs(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys) :
            _OldKeys{ oldKeys },
            _NewKeys{ newKeys } {}

        WINRT_PROPERTY(Control::KeyChord, OldKeys, nullptr);
        WINRT_PROPERTY(Control::KeyChord, NewKeys, nullptr);
    };

    struct CommandViewModel : CommandViewModelT<CommandViewModel>, ViewModelHelper<CommandViewModel>
    {
    public:
        CommandViewModel(const winrt::Microsoft::Terminal::Settings::Model::Command& cmd,
                         std::vector<Control::KeyChord> keyChordList,
                         const Editor::ActionsViewModel& actionsPageVM,
                         Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> availableActionsAndNamesMap,
                         Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> nameToActionMap);
        void Initialize();

        winrt::hstring DisplayName();
        winrt::hstring Name();
        void Name(const winrt::hstring& newName);
        winrt::hstring DisplayNameAndKeyChordAutomationPropName();

        winrt::hstring FirstKeyChordText();
        winrt::hstring AdditionalKeyChordCountText();
        winrt::hstring AdditionalKeyChordTooltipText();

        winrt::hstring ID();
        bool IsUserAction();

        void Edit_Click();
        til::typed_event<Editor::CommandViewModel, IInspectable> EditRequested;

        void Delete_Click();
        til::typed_event<Editor::CommandViewModel, IInspectable> DeleteRequested;

        void AddKeybinding_Click();

        // UIA text
        winrt::hstring ActionNameTextBoxAutomationPropName();
        winrt::hstring ShortcutActionComboBoxAutomationPropName();
        winrt::hstring AdditionalArgumentsControlAutomationPropName();

        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeNamesRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateWindowRootRequested;
        til::typed_event<IInspectable, IInspectable> FocusContainer;

        VIEW_MODEL_OBSERVABLE_PROPERTY(IInspectable, ProposedShortcutActionName);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ActionArgsViewModel, ActionArgsVM, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<hstring>, AvailableShortcutActions, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::KeyChordViewModel>, KeyChordList, nullptr);
        WINRT_PROPERTY(bool, IsNewCommand, false);

    private:
        winrt::hstring _cachedDisplayName;
        winrt::Microsoft::Terminal::Settings::Model::Command _command;
        std::vector<Control::KeyChord> _keyChordList;
        weak_ref<Editor::ActionsViewModel> _actionsPageVM{ nullptr };
        Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> _availableActionsAndNamesMap;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> _nameToActionMap;
        void _RegisterKeyChordVMEvents(Editor::KeyChordViewModel kcVM);
        void _RegisterActionArgsVMEvents(Editor::ActionArgsViewModel actionArgsVM);
        void _ReplaceCommandWithUserCopy(bool reinitialize);
        void _CreateAndInitializeActionArgsVMHelper();
    };

    struct ArgWrapper : ArgWrapperT<ArgWrapper>, ViewModelHelper<ArgWrapper>
    {
    public:
        ArgWrapper(const Model::ArgDescriptor& descriptor, const Windows::Foundation::IInspectable& value);
        void Initialize();

        winrt::hstring Name() const noexcept { return _descriptor.Name; };
        winrt::hstring Type() const noexcept { return _descriptor.Type; };
        Model::ArgTypeHint TypeHint() const noexcept { return _descriptor.TypeHint; };
        bool Required() const noexcept { return _descriptor.Required; };
        Editor::IHostedInWindow WindowRoot() const noexcept { return _WeakWindowRoot.get(); }
        void WindowRoot(const Editor::IHostedInWindow& value) { _WeakWindowRoot = value; }

        // We cannot use the macro here because we need to implement additional logic for the setter
        Windows::Foundation::IInspectable EnumValue() const noexcept { return _EnumValue; };
        void EnumValue(const Windows::Foundation::IInspectable& value);
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry> EnumList() const noexcept { return _EnumList; };
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::FlagEntry> FlagList() const noexcept { return _FlagList; };

        // unboxing functions
        winrt::hstring UnboxString(const Windows::Foundation::IInspectable& value);
        int32_t UnboxInt32(const Windows::Foundation::IInspectable& value);
        float UnboxInt32Optional(const Windows::Foundation::IInspectable& value);
        uint32_t UnboxUInt32(const Windows::Foundation::IInspectable& value);
        float UnboxUInt32Optional(const Windows::Foundation::IInspectable& value);
        float UnboxFloat(const Windows::Foundation::IInspectable& value);
        bool UnboxBool(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<bool> UnboxBoolOptional(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> UnboxTerminalCoreColorOptional(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> UnboxWindowsUIColorOptional(const Windows::Foundation::IInspectable& value);

        // bind back functions
        void StringBindBack(const winrt::hstring& newValue);
        void Int32BindBack(const double newValue);
        void Int32OptionalBindBack(const double newValue);
        void UInt32BindBack(const double newValue);
        void UInt32OptionalBindBack(const double newValue);
        void FloatBindBack(const double newValue);
        void BoolOptionalBindBack(const Windows::Foundation::IReference<bool> newValue);
        void TerminalCoreColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue);
        void WindowsUIColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue);

        safe_void_coroutine BrowseForFile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        safe_void_coroutine BrowseForFolder_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        // some argWrappers need to know additional information (like the default color scheme or the list of all color scheme names)
        // to avoid populating all ArgWrappers with that information, instead we emit an event when we need that information
        // (these events then get propagated up to the ActionsVM) and then the actionsVM will populate the value in us
        // since there's an actionArgsVM above us and a commandVM above that, the event does get propagated through a few times but that's
        // probably better than having every argWrapper contain the information by default
        til::typed_event<IInspectable, Editor::ArgWrapper> ColorSchemeRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> ColorSchemeNamesRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> WindowRootRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, DefaultColorScheme, nullptr);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::Foundation::IInspectable, Value, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<winrt::hstring>, ColorSchemeNamesList, nullptr);

    private:
        Model::ArgDescriptor _descriptor;
        winrt::weak_ref<Editor::IHostedInWindow> _WeakWindowRoot{ nullptr };
        Windows::Foundation::IInspectable _EnumValue{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry> _EnumList;
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::FlagEntry> _FlagList;

        template<typename EnumType>
        void _InitializeEnumListAndValue(
            const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
            const winrt::hstring& resourceSectionAndType,
            const winrt::hstring& resourceProperty,
            const bool nullable);

        template<typename EnumType>
        void _InitializeFlagListAndValue(
            const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, EnumType>& mappings,
            const winrt::hstring& resourceSectionAndType,
            const winrt::hstring& resourceProperty,
            const bool nullable);
    };

    struct ActionArgsViewModel : ActionArgsViewModelT<ActionArgsViewModel>, ViewModelHelper<ActionArgsViewModel>
    {
    public:
        ActionArgsViewModel(const Microsoft::Terminal::Settings::Model::ActionAndArgs actionAndArgs);
        void Initialize();

        bool HasArgs() const noexcept;
        void ReplaceActionAndArgs(Model::ActionAndArgs newActionAndArgs);

        til::typed_event<IInspectable, IInspectable> WrapperValueChanged;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeNamesRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateWindowRootRequested;

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ArgWrapper>, ArgValues, nullptr);

    private:
        Model::ActionAndArgs _actionAndArgs{ nullptr };
    };

    struct KeyChordViewModel : KeyChordViewModelT<KeyChordViewModel>, ViewModelHelper<KeyChordViewModel>
    {
    public:
        KeyChordViewModel(Control::KeyChord CurrentKeys);

        void CurrentKeys(const Control::KeyChord& newKeys);
        Control::KeyChord CurrentKeys() const noexcept;

        void ToggleEditMode();
        void AcceptChanges();
        void CancelChanges();
        void DeleteKeyChord();

        // UIA Text
        hstring CancelButtonName() const noexcept;
        hstring AcceptButtonName() const noexcept;
        hstring DeleteButtonName() const noexcept;

        VIEW_MODEL_OBSERVABLE_PROPERTY(bool, IsInEditMode, false);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Control::KeyChord, ProposedKeys);
        VIEW_MODEL_OBSERVABLE_PROPERTY(winrt::hstring, KeyChordText);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::Flyout, AcceptChangesFlyout, nullptr);

    public:
        til::typed_event<Editor::KeyChordViewModel, Terminal::Control::KeyChord> AddKeyChordRequested;
        til::typed_event<Editor::KeyChordViewModel, Editor::ModifyKeyChordEventArgs> ModifyKeyChordRequested;
        til::typed_event<Editor::KeyChordViewModel, Terminal::Control::KeyChord> DeleteKeyChordRequested;

    private:
        Control::KeyChord _currentKeys;
    };

    struct ActionsViewModel : ActionsViewModelT<ActionsViewModel>, ViewModelHelper<ActionsViewModel>
    {
    public:
        ActionsViewModel(Model::CascadiaSettings settings);
        void UpdateSettings(const Model::CascadiaSettings& settings);
        void MarkAsVisited();
        bool DisplayBadge() const noexcept;

        void AddNewCommand();

        void CurrentCommand(const Editor::CommandViewModel& newCommand);
        Editor::CommandViewModel CurrentCommand();
        void CmdListItemClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs& e);

        void DeleteKeyChord(const Control::KeyChord& keys);
        void AttemptAddOrModifyKeyChord(const Editor::KeyChordViewModel& senderVM, winrt::hstring commandID, const Control::KeyChord& newKeys, const Control::KeyChord& oldKeys);
        void AddCopiedCommand(const Model::Command& newCommand);
        void RegenerateCommandID(const Model::Command& command);

        Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> AvailableShortcutActionsAndNames();
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> NameToActionMap();

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::CommandViewModel>, CommandList);
        WINRT_OBSERVABLE_PROPERTY(ActionsSubPage, CurrentPage, _propertyChangedHandlers, ActionsSubPage::Base);

    private:
        Editor::CommandViewModel _CurrentCommand{ nullptr };
        Model::CascadiaSettings _Settings;
        Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> _AvailableActionsAndNamesMap;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ShortcutAction> _NameToActionMap;

        void _MakeCommandVMsHelper();
        void _RegisterCmdVMEvents(com_ptr<implementation::CommandViewModel>& cmdVM);

        void _CmdVMEditRequestedHandler(const Editor::CommandViewModel& senderVM, const IInspectable& args);
        void _CmdVMDeleteRequestedHandler(const Editor::CommandViewModel& senderVM, const IInspectable& args);
        void _CmdVMPropagateColorSchemeRequestedHandler(const IInspectable& sender, const Editor::ArgWrapper& wrapper);
        void _CmdVMPropagateColorSchemeNamesRequestedHandler(const IInspectable& sender, const Editor::ArgWrapper& wrapper);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ActionsViewModel);
}
