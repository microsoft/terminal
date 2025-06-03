// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ActionsViewModel.g.h"
#include "NavigateToCommandArgs.g.h"
#include "CommandViewModel.g.h"
#include "ArgWrapper.g.h"
#include "ActionArgsViewModel.g.h"
#include "KeyChordViewModel.g.h"
#include "ModifyKeyChordEventArgs.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct CommandViewModelComparator
    {
        bool operator()(const Editor::CommandViewModel& lhs, const Editor::CommandViewModel& rhs) const
        {
            return lhs.DisplayName() < rhs.DisplayName();
        }
    };

    struct NavigateToCommandArgs : NavigateToCommandArgsT<NavigateToCommandArgs>
    {
    public:
        NavigateToCommandArgs(CommandViewModel command, Editor::IHostedInWindow windowRoot) :
            _Command(command),
            _WindowRoot(windowRoot) {}

        Editor::IHostedInWindow WindowRoot() const noexcept { return _WindowRoot; }
        Editor::CommandViewModel Command() const noexcept { return _Command; }

    private:
        Editor::IHostedInWindow _WindowRoot;
        Editor::CommandViewModel _Command{ nullptr };
    };

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
        CommandViewModel(winrt::Microsoft::Terminal::Settings::Model::Command cmd,
                         std::vector<Control::KeyChord> keyChordList,
                         const Editor::ActionsViewModel actionsPageVM,
                         const Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring>& availableShortcutActionsAndNames);
        void Initialize();

        winrt::hstring DisplayName();
        winrt::hstring Name();
        void Name(const winrt::hstring& newName);

        winrt::hstring ID();
        void ID(const winrt::hstring& newID);

        bool IsUserAction();

        void Edit_Click();
        til::typed_event<Editor::CommandViewModel, IInspectable> EditRequested;

        void Delete_Click();
        til::typed_event<Editor::CommandViewModel, IInspectable> DeleteRequested;

        void AddKeybinding_Click();

        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateColorSchemeNamesRequested;
        til::typed_event<IInspectable, Editor::ArgWrapper> PropagateWindowRootRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(IInspectable, ProposedShortcutAction);
        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ActionArgsViewModel, ActionArgsVM, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<hstring>, AvailableShortcutActions, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::KeyChordViewModel>, KeyChordViewModelList, nullptr);
        WINRT_PROPERTY(bool, IsNewCommand, false);

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command;
        std::vector<Control::KeyChord> _keyChordList;
        weak_ref<Editor::ActionsViewModel> _actionsPageVM{ nullptr };
        void _RegisterKeyChordVMEvents(Editor::KeyChordViewModel kcVM);
        void _RegisterActionArgsVMEvents(Editor::ActionArgsViewModel actionArgsVM);
        void _ReplaceCommandWithUserCopy(bool reinitialize);
        void _CreateAndInitializeActionArgsVMHelper();
        Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> _AvailableActionsAndNamesMap;
        std::unordered_map<winrt::hstring, Model::ShortcutAction> _NameToActionMap;
    };

    struct ArgWrapper : ArgWrapperT<ArgWrapper>, ViewModelHelper<ArgWrapper>
    {
    public:
        ArgWrapper(const winrt::hstring& name, const winrt::hstring& type, const bool required, const Model::ArgTypeHint typeHint, const Windows::Foundation::IInspectable& value);
        void Initialize();

        winrt::hstring Name() const noexcept { return _name; };
        winrt::hstring Type() const noexcept { return _type; };
        Model::ArgTypeHint TypeHint() const noexcept { return _typeHint; };
        bool Required() const noexcept { return _required; };

        // We cannot use the macro here because we need to implement additional logic for the setter
        Windows::Foundation::IInspectable EnumValue() const noexcept { return _EnumValue; };
        void EnumValue(const Windows::Foundation::IInspectable& value);
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry> EnumList() const noexcept { return _EnumList; };
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::FlagEntry> FlagList() const noexcept { return _FlagList; };

        // unboxing functions
        winrt::hstring UnboxString(const Windows::Foundation::IInspectable& value);
        winrt::hstring UnboxGuid(const Windows::Foundation::IInspectable& value);
        int32_t UnboxInt32(const Windows::Foundation::IInspectable& value);
        float UnboxInt32Optional(const Windows::Foundation::IInspectable& value);
        uint32_t UnboxUInt32(const Windows::Foundation::IInspectable& value);
        float UnboxUInt32Optional(const Windows::Foundation::IInspectable& value);
        float UnboxUInt64(const Windows::Foundation::IInspectable& value);
        float UnboxFloat(const Windows::Foundation::IInspectable& value);
        bool UnboxBool(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<bool> UnboxBoolOptional(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> UnboxTerminalCoreColorOptional(const Windows::Foundation::IInspectable& value);
        winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> UnboxWindowsUIColorOptional(const Windows::Foundation::IInspectable& value);

        // bind back functions
        void StringBindBack(const winrt::hstring& newValue);
        void GuidBindBack(const winrt::hstring& newValue);
        void Int32BindBack(const double newValue);
        void Int32OptionalBindBack(const double newValue);
        void UInt32BindBack(const double newValue);
        void UInt32OptionalBindBack(const double newValue);
        void UInt64BindBack(const double newValue);
        void FloatBindBack(const double newValue);
        void BoolOptionalBindBack(const Windows::Foundation::IReference<bool> newValue);
        void TerminalCoreColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue);
        void WindowsUIColorBindBack(const winrt::Windows::Foundation::IReference<Microsoft::Terminal::Core::Color> newValue);

        safe_void_coroutine Browse_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

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
        WINRT_PROPERTY(Editor::IHostedInWindow, WindowRoot, nullptr);

    private:
        winrt::hstring _name;
        winrt::hstring _type;
        Model::ArgTypeHint _typeHint;
        bool _required;
        Windows::Foundation::IInspectable _EnumValue{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry> _EnumList;
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::FlagEntry> _FlagList;
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
        void AttemptAcceptChanges();
        void CancelChanges();
        void DeleteKeyChord();

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

        void AddNewCommand();

        void CurrentCommand(const Editor::CommandViewModel& newCommand);
        Editor::CommandViewModel CurrentCommand();
        void CmdListItemClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs& e);

        void AttemptDeleteKeyChord(const Control::KeyChord& keys);
        void AttemptAddOrModifyKeyChord(const Editor::KeyChordViewModel& senderVM, winrt::hstring commandID, const Control::KeyChord& newKeys, const Control::KeyChord& oldKeys);
        void AttemptAddCopiedCommand(const Model::Command& newCommand);

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::CommandViewModel>, CommandList);
        WINRT_OBSERVABLE_PROPERTY(ActionsSubPage, CurrentPage, _propertyChangedHandlers, ActionsSubPage::Base);

    private:
        Editor::CommandViewModel _CurrentCommand{ nullptr };
        Model::CascadiaSettings _Settings;
        Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> _AvailableActionsAndNamesMap;

        void _MakeCommandVMsHelper();
        void _RegisterCmdVMEvents(com_ptr<implementation::CommandViewModel>& cmdVM);

        void _CmdVMPropertyChangedHandler(const IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
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
