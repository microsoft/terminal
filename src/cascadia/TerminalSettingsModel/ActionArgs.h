// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// HEY YOU: When adding ActionArgs types, make sure to add the corresponding
//          *.g.cpp to ActionArgs.cpp!
#include "ActionEventArgs.g.h"
#include "BaseContentArgs.g.h"
#include "NewTerminalArgs.g.h"
#include "CopyTextArgs.g.h"
#include "NewTabArgs.g.h"
#include "SwitchToTabArgs.g.h"
#include "ResizePaneArgs.g.h"
#include "MoveFocusArgs.g.h"
#include "MovePaneArgs.g.h"
#include "SwapPaneArgs.g.h"
#include "AdjustFontSizeArgs.g.h"
#include "SendInputArgs.g.h"
#include "SplitPaneArgs.g.h"
#include "OpenSettingsArgs.g.h"
#include "SetFocusModeArgs.g.h"
#include "SetFullScreenArgs.g.h"
#include "SetMaximizedArgs.g.h"
#include "SetColorSchemeArgs.g.h"
#include "SetTabColorArgs.g.h"
#include "RenameTabArgs.g.h"
#include "ExecuteCommandlineArgs.g.h"
#include "CloseOtherTabsArgs.g.h"
#include "CloseTabsAfterArgs.g.h"
#include "CloseTabArgs.g.h"
#include "ScrollUpArgs.g.h"
#include "ScrollDownArgs.g.h"
#include "ScrollToMarkArgs.g.h"
#include "AddMarkArgs.g.h"
#include "MoveTabArgs.g.h"
#include "SaveSnippetArgs.g.h"
#include "ToggleCommandPaletteArgs.g.h"
#include "SuggestionsArgs.g.h"
#include "FindMatchArgs.g.h"
#include "NewWindowArgs.g.h"
#include "PrevTabArgs.g.h"
#include "NextTabArgs.g.h"
#include "RenameWindowArgs.g.h"
#include "SearchForTextArgs.g.h"
#include "GlobalSummonArgs.g.h"
#include "FocusPaneArgs.g.h"
#include "ExportBufferArgs.g.h"
#include "ClearBufferArgs.g.h"
#include "MultipleActionsArgs.g.h"
#include "AdjustOpacityArgs.g.h"
#include "SelectCommandArgs.g.h"
#include "SelectOutputArgs.g.h"
#include "ColorSelectionArgs.g.h"

#include "JsonUtils.h"
#include "HashUtils.h"
#include "TerminalWarnings.h"
#include "../WinRTUtils/inc/LibraryResources.h"

#include "TerminalSettingsSerializationHelpers.h"

#include <LibraryResources.h>
#include <ScopedResourceLoader.h>

#include "ActionArgsMagic.h"

#define ACTION_ARG(type, name, ...)                                         \
public:                                                                     \
    type name() const noexcept                                              \
    {                                                                       \
        return _##name.has_value() ? _##name.value() : type{ __VA_ARGS__ }; \
    }                                                                       \
    void name(const type& value) noexcept                                   \
    {                                                                       \
        _##name = value;                                                    \
    }                                                                       \
                                                                            \
protected:                                                                  \
    std::optional<type> _##name{ std::nullopt };

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as an ActionArgs
//   class that implements IActionArgs
// * ActionEventArgs holds a single IActionArgs. For events that don't need
//   additional args, this can be nullptr.

////////////////////////////////////////////////////////////////////////////////
// BEGIN X-MACRO MADNESS
////////////////////////////////////////////////////////////////////////////////

// A helper for saying "This class has no other property validation". This will
// make sense below.
#define NO_OTHER_VALIDATION() ;

// Below are definitions for the properties for each and every ActionArgs type.
// Each should maintain the same format:
//
// #define MY_FOO_ARGS(X)                    \
//     X(ParamOneType, One, "one", {validation}, {default args}) \
//     X(Windows::Foundation::IReference<ParamTwoType>, Two, "two", {validation}, nullptr)
//     { etc... }
//
// If one of your properties needs some additional validation done to it, then
// fill in {validation} with whatever logic needs to be done. Or, just set to
// false, if we don't really care if the parameter is required or not.

////////////////////////////////////////////////////////////////////////////////
#define COPY_TEXT_ARGS(X)                                                                  \
    X(bool, DismissSelection, "dismissSelection", false, ArgTypeHint::None, true)          \
    X(bool, SingleLine, "singleLine", false, ArgTypeHint::None, false)                     \
    X(bool, WithControlSequences, "withControlSequences", false, ArgTypeHint::None, false) \
    X(Windows::Foundation::IReference<Control::CopyFormat>, CopyFormatting, "copyFormatting", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define MOVE_PANE_ARGS(X)                                       \
    X(uint32_t, TabIndex, "index", false, ArgTypeHint::None, 0) \
    X(winrt::hstring, Window, "window", false, ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define SWITCH_TO_TAB_ARGS(X) \
    X(uint32_t, TabIndex, "index", false, ArgTypeHint::None, 0)

////////////////////////////////////////////////////////////////////////////////
#define RESIZE_PANE_ARGS(X) \
    X(Model::ResizeDirection, ResizeDirection, "direction", args->ResizeDirection() == ResizeDirection::None, ArgTypeHint::None, Model::ResizeDirection::None)

////////////////////////////////////////////////////////////////////////////////
#define MOVE_FOCUS_ARGS(X) \
    X(Model::FocusDirection, FocusDirection, "direction", args->FocusDirection() == Model::FocusDirection::None, ArgTypeHint::None, Model::FocusDirection::None)

////////////////////////////////////////////////////////////////////////////////
#define SWAP_PANE_ARGS(X) \
    X(Model::FocusDirection, Direction, "direction", args->Direction() == Model::FocusDirection::None, ArgTypeHint::None, Model::FocusDirection::None)

////////////////////////////////////////////////////////////////////////////////
#define ADJUST_FONT_SIZE_ARGS(X) \
    X(float, Delta, "delta", false, ArgTypeHint::None, 0)

////////////////////////////////////////////////////////////////////////////////
#define SEND_INPUT_ARGS(X) \
    X(winrt::hstring, Input, "input", args->Input().empty(), ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define OPEN_SETTINGS_ARGS(X) \
    X(SettingsTarget, Target, "target", false, ArgTypeHint::None, SettingsTarget::SettingsFile)

////////////////////////////////////////////////////////////////////////////////
#define SET_FOCUS_MODE_ARGS(X) \
    X(bool, IsFocusMode, "isFocusMode", false, ArgTypeHint::None, false)

////////////////////////////////////////////////////////////////////////////////
#define SET_MAXIMIZED_ARGS(X) \
    X(bool, IsMaximized, "isMaximized", false, ArgTypeHint::None, false)

////////////////////////////////////////////////////////////////////////////////
#define SET_FULL_SCREEN_ARGS(X) \
    X(bool, IsFullScreen, "isFullScreen", false, ArgTypeHint::None, false)

////////////////////////////////////////////////////////////////////////////////
#define SET_MAXIMIZED_ARGS(X) \
    X(bool, IsMaximized, "isMaximized", false, ArgTypeHint::None, false)

////////////////////////////////////////////////////////////////////////////////
#define SET_COLOR_SCHEME_ARGS(X) \
    X(winrt::hstring, SchemeName, "colorScheme", args->SchemeName().empty(), ArgTypeHint::ColorScheme, L"")

////////////////////////////////////////////////////////////////////////////////
#define SET_TAB_COLOR_ARGS(X) \
    X(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, "color", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define RENAME_TAB_ARGS(X) \
    X(winrt::hstring, Title, "title", false, ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define EXECUTE_COMMANDLINE_ARGS(X) \
    X(winrt::hstring, Commandline, "commandline", args->Commandline().empty(), ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define CLOSE_OTHER_TABS_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define CLOSE_TABS_AFTER_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define CLOSE_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
// Interestingly, the order MATTERS here. Window has to be BEFORE Direction,
// because otherwise we won't have parsed the Window yet when we validate the
// Direction.
#define MOVE_TAB_ARGS(X)                                               \
    X(winrt::hstring, Window, "window", false, ArgTypeHint::None, L"") \
    X(MoveTabDirection, Direction, "direction", (args->Direction() == MoveTabDirection::None) && (args->Window().empty()), ArgTypeHint::None, MoveTabDirection::None)

// Other ideas:
//  X(uint32_t, TabIndex, "index", false, 0) \ // target? source?

////////////////////////////////////////////////////////////////////////////////
#define SCROLL_UP_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, RowsToScroll, "rowsToScroll", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define SCROLL_DOWN_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, RowsToScroll, "rowsToScroll", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define SCROLL_TO_MARK_ARGS(X) \
    X(Microsoft::Terminal::Control::ScrollToMarkDirection, Direction, "direction", false, ArgTypeHint::None, Microsoft::Terminal::Control::ScrollToMarkDirection::Previous)

////////////////////////////////////////////////////////////////////////////////
#define ADD_MARK_ARGS(X) \
    X(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, Color, "color", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define TOGGLE_COMMAND_PALETTE_ARGS(X) \
    X(CommandPaletteLaunchMode, LaunchMode, "launchMode", false, ArgTypeHint::None, CommandPaletteLaunchMode::Action)

////////////////////////////////////////////////////////////////////////////////
#define SAVE_TASK_ARGS(X)                                                                              \
    X(winrt::hstring, Name, "name", false, ArgTypeHint::None, L"")                                     \
    X(winrt::hstring, Commandline, "commandline", args->Commandline().empty(), ArgTypeHint::None, L"") \
    X(winrt::hstring, KeyChord, "keyChord", false, ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define SUGGESTIONS_ARGS(X)                                                                    \
    X(SuggestionsSource, Source, "source", false, ArgTypeHint::None, SuggestionsSource::Tasks) \
    X(bool, UseCommandline, "useCommandline", false, ArgTypeHint::None, false)

////////////////////////////////////////////////////////////////////////////////
#define FIND_MATCH_ARGS(X) \
    X(FindMatchDirection, Direction, "direction", args->Direction() == FindMatchDirection::None, ArgTypeHint::None, FindMatchDirection::None)

////////////////////////////////////////////////////////////////////////////////
#define PREV_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, "tabSwitcherMode", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define NEXT_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, "tabSwitcherMode", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define RENAME_WINDOW_ARGS(X) \
    X(winrt::hstring, Name, "name", false, ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define SEARCH_FOR_TEXT_ARGS(X) \
    X(winrt::hstring, QueryUrl, "queryUrl", false, ArgTypeHint::None, L"")

////////////////////////////////////////////////////////////////////////////////
#define GLOBAL_SUMMON_ARGS(X)                                                                                  \
    X(winrt::hstring, Name, "name", false, ArgTypeHint::None, L"")                                             \
    X(Model::DesktopBehavior, Desktop, "desktop", false, ArgTypeHint::None, Model::DesktopBehavior::ToCurrent) \
    X(Model::MonitorBehavior, Monitor, "monitor", false, ArgTypeHint::None, Model::MonitorBehavior::ToMouse)   \
    X(bool, ToggleVisibility, "toggleVisibility", false, ArgTypeHint::None, true)                              \
    X(uint32_t, DropdownDuration, "dropdownDuration", false, ArgTypeHint::None, 0)

////////////////////////////////////////////////////////////////////////////////
#define FOCUS_PANE_ARGS(X) \
    X(uint32_t, Id, "id", false, ArgTypeHint::None, 0u)

////////////////////////////////////////////////////////////////////////////////
#define EXPORT_BUFFER_ARGS(X) \
    X(winrt::hstring, Path, "path", false, ArgTypeHint::FilePath, L"")

////////////////////////////////////////////////////////////////////////////////
#define CLEAR_BUFFER_ARGS(X) \
    X(winrt::Microsoft::Terminal::Control::ClearBufferType, Clear, "clear", false, ArgTypeHint::None, winrt::Microsoft::Terminal::Control::ClearBufferType::All)

////////////////////////////////////////////////////////////////////////////////
#define ADJUST_OPACITY_ARGS(X)                                  \
    X(int32_t, Opacity, "opacity", false, ArgTypeHint::None, 0) \
    X(bool, Relative, "relative", false, ArgTypeHint::None, true)

////////////////////////////////////////////////////////////////////////////////
#define SELECT_COMMAND_ARGS(X) \
    X(SelectOutputDirection, Direction, "direction", false, ArgTypeHint::None, SelectOutputDirection::Previous)

////////////////////////////////////////////////////////////////////////////////
#define SELECT_OUTPUT_ARGS(X) \
    X(SelectOutputDirection, Direction, "direction", false, ArgTypeHint::None, SelectOutputDirection::Previous)

////////////////////////////////////////////////////////////////////////////////
#define COLOR_SELECTION_ARGS(X)                                                                                         \
    X(winrt::Microsoft::Terminal::Control::SelectionColor, Foreground, "foreground", false, ArgTypeHint::None, nullptr) \
    X(winrt::Microsoft::Terminal::Control::SelectionColor, Background, "background", false, ArgTypeHint::None, nullptr) \
    X(winrt::Microsoft::Terminal::Core::MatchMode, MatchMode, "matchMode", false, ArgTypeHint::None, winrt::Microsoft::Terminal::Core::MatchMode::None)

////////////////////////////////////////////////////////////////////////////////
#define NEW_TERMINAL_ARGS(X)                                                                                                          \
    X(winrt::hstring, Commandline, "commandline", false, ArgTypeHint::None, L"")                                                      \
    X(winrt::hstring, StartingDirectory, "startingDirectory", false, ArgTypeHint::FolderPath, L"")                                    \
    X(winrt::hstring, TabTitle, "tabTitle", false, ArgTypeHint::None, L"")                                                            \
    X(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, "tabColor", false, ArgTypeHint::None, nullptr)                   \
    X(Windows::Foundation::IReference<int32_t>, ProfileIndex, "index", false, ArgTypeHint::None, nullptr)                             \
    X(winrt::hstring, Profile, "profile", false, ArgTypeHint::None, L"")                                                              \
    X(Windows::Foundation::IReference<bool>, SuppressApplicationTitle, "suppressApplicationTitle", false, ArgTypeHint::None, nullptr) \
    X(winrt::hstring, ColorScheme, "colorScheme", args->SchemeName().empty(), ArgTypeHint::ColorScheme, L"")                          \
    X(Windows::Foundation::IReference<bool>, Elevate, "elevate", false, ArgTypeHint::None, nullptr)                                   \
    X(Windows::Foundation::IReference<bool>, ReloadEnvironmentVariables, "reloadEnvironmentVariables", false, ArgTypeHint::None, nullptr)

////////////////////////////////////////////////////////////////////////////////
#define SPLIT_PANE_ARGS(X)                                                                                 \
    X(Model::SplitDirection, SplitDirection, "split", false, ArgTypeHint::None, SplitDirection::Automatic) \
    X(SplitType, SplitMode, "splitMode", false, ArgTypeHint::None, SplitType::Manual)                      \
    X(float, SplitSize, "size", false, ArgTypeHint::None, 0.5f)

////////////////////////////////////////////////////////////////////////////////

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    using namespace ::Microsoft::Terminal::Settings::Model;
    using FromJsonResult = std::tuple<Model::IActionArgs, std::vector<SettingsLoadWarnings>>;

    struct ActionEventArgs : public ActionEventArgsT<ActionEventArgs>
    {
        ActionEventArgs() = default;

        explicit ActionEventArgs(const Model::IActionArgs& args) :
            _ActionArgs{ args } {};
        WINRT_PROPERTY(IActionArgs, ActionArgs, nullptr);
        WINRT_PROPERTY(bool, Handled, false);
    };

    struct BaseContentArgs : public BaseContentArgsT<BaseContentArgs>
    {
        BaseContentArgs(winrt::hstring type) :
            _Type{ type } {}

        BaseContentArgs() :
            BaseContentArgs(L"") {}

        ACTION_ARG(winrt::hstring, Type, L"");

        static constexpr std::string_view TypeKey{ "type" };

    public:
        bool Equals(INewContentArgs other) const
        {
            return other.Type() == _Type;
        }
        size_t Hash() const
        {
            til::hasher h;
            Hash(h);
            return h.finalize();
        }
        void Hash(til::hasher& h) const
        {
            h.write(Type());
        }
        INewContentArgs Copy() const
        {
            auto copy{ winrt::make_self<BaseContentArgs>() };
            copy->_Type = _Type;
            return *copy;
        }
        winrt::hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        winrt::hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const
        {
            return winrt::hstring{ L"type: " } + Type();
        }
        static Json::Value ToJson(const Model::BaseContentArgs& val)
        {
            if (!val)
            {
                return {};
            }
            auto args{ get_self<BaseContentArgs>(val) };
            Json::Value json{ Json::ValueType::objectValue };
            JsonUtils::SetValueForKey(json, TypeKey, args->_Type);
            return json;
        }
    };

    // Although it may _seem_ like NewTerminalArgs can use ACTION_ARG_BODY, it
    // actually can't, because it isn't an `IActionArgs`, which breaks some
    // assumptions made in the macro.
    struct NewTerminalArgs : public NewTerminalArgsT<NewTerminalArgs>
    {
        PARTIAL_ACTION_ARG_BODY(NewTerminalArgs, NEW_TERMINAL_ARGS);
        ACTION_ARG(winrt::hstring, Type, L"");
        ACTION_ARG(winrt::guid, SessionId, winrt::guid{});
        ACTION_ARG(bool, AppendCommandLine, false);
        ACTION_ARG(uint64_t, ContentId);

        static constexpr std::string_view SessionIdKey{ "sessionId" };
        static constexpr std::string_view AppendCommandLineKey{ "appendCommandLine" };
        static constexpr std::string_view ContentKey{ "__content" };

    public:
        NewTerminalArgs(int32_t& profileIndex) :
            _ProfileIndex{ profileIndex } {};
        hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const;
        hstring ToCommandline() const;

        bool Equals(const Model::INewContentArgs& other)
        {
            auto otherAsUs = other.try_as<NewTerminalArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Commandline == _Commandline &&
                       otherAsUs->_StartingDirectory == _StartingDirectory &&
                       otherAsUs->_TabTitle == _TabTitle &&
                       otherAsUs->_TabColor == _TabColor &&
                       otherAsUs->_ProfileIndex == _ProfileIndex &&
                       otherAsUs->_Profile == _Profile &&
                       otherAsUs->_AppendCommandLine == _AppendCommandLine &&
                       otherAsUs->_SuppressApplicationTitle == _SuppressApplicationTitle &&
                       otherAsUs->_ColorScheme == _ColorScheme &&
                       otherAsUs->_Elevate == _Elevate &&
                       otherAsUs->_ReloadEnvironmentVariables == _ReloadEnvironmentVariables &&
                       otherAsUs->_ContentId == _ContentId;
            }
            return false;
        };
        static Model::NewTerminalArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewTerminalArgs>();
            JsonUtils::GetValueForKey(json, CommandlineKey, args->_Commandline);
            JsonUtils::GetValueForKey(json, StartingDirectoryKey, args->_StartingDirectory);
            JsonUtils::GetValueForKey(json, TabTitleKey, args->_TabTitle);
            JsonUtils::GetValueForKey(json, ProfileIndexKey, args->_ProfileIndex);
            JsonUtils::GetValueForKey(json, ProfileKey, args->_Profile);
            JsonUtils::GetValueForKey(json, SessionIdKey, args->_SessionId);
            JsonUtils::GetValueForKey(json, TabColorKey, args->_TabColor);
            JsonUtils::GetValueForKey(json, SuppressApplicationTitleKey, args->_SuppressApplicationTitle);
            JsonUtils::GetValueForKey(json, ColorSchemeKey, args->_ColorScheme);
            JsonUtils::GetValueForKey(json, ElevateKey, args->_Elevate);
            JsonUtils::GetValueForKey(json, ReloadEnvironmentVariablesKey, args->_ReloadEnvironmentVariables);
            JsonUtils::GetValueForKey(json, ContentKey, args->_ContentId);
            return *args;
        }
        static Json::Value ToJson(const Model::NewTerminalArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<NewTerminalArgs>(val) };
            JsonUtils::SetValueForKey(json, CommandlineKey, args->_Commandline);
            JsonUtils::SetValueForKey(json, StartingDirectoryKey, args->_StartingDirectory);
            JsonUtils::SetValueForKey(json, TabTitleKey, args->_TabTitle);
            JsonUtils::SetValueForKey(json, ProfileIndexKey, args->_ProfileIndex);
            JsonUtils::SetValueForKey(json, ProfileKey, args->_Profile);
            JsonUtils::SetValueForKey(json, SessionIdKey, args->_SessionId);
            JsonUtils::SetValueForKey(json, TabColorKey, args->_TabColor);
            JsonUtils::SetValueForKey(json, SuppressApplicationTitleKey, args->_SuppressApplicationTitle);
            JsonUtils::SetValueForKey(json, ColorSchemeKey, args->_ColorScheme);
            JsonUtils::SetValueForKey(json, ElevateKey, args->_Elevate);
            JsonUtils::SetValueForKey(json, ReloadEnvironmentVariablesKey, args->_ReloadEnvironmentVariables);
            JsonUtils::SetValueForKey(json, ContentKey, args->_ContentId);
            return json;
        }
        Model::NewTerminalArgs Copy() const
        {
            auto copy{ winrt::make_self<NewTerminalArgs>() };
            copy->_Commandline = _Commandline;
            copy->_StartingDirectory = _StartingDirectory;
            copy->_TabTitle = _TabTitle;
            copy->_TabColor = _TabColor;
            copy->_ProfileIndex = _ProfileIndex;
            copy->_Profile = _Profile;
            copy->_SessionId = _SessionId;
            copy->_SuppressApplicationTitle = _SuppressApplicationTitle;
            copy->_ColorScheme = _ColorScheme;
            copy->_Elevate = _Elevate;
            copy->_ReloadEnvironmentVariables = _ReloadEnvironmentVariables;
            copy->_ContentId = _ContentId;
            return *copy;
        }
        size_t Hash() const
        {
            til::hasher h;
            Hash(h);
            return h.finalize();
        }
        void Hash(til::hasher& h) const
        {
            h.write(Commandline());
            h.write(StartingDirectory());
            h.write(TabTitle());
            h.write(TabColor());
            h.write(ProfileIndex());
            h.write(Profile());
            h.write(SuppressApplicationTitle());
            h.write(ColorScheme());
            h.write(Elevate());
            h.write(ReloadEnvironmentVariables());
            h.write(ContentId());
        }
    };

    static std::tuple<Model::INewContentArgs, std::vector<SettingsLoadWarnings>> ContentArgsFromJson(const Json::Value& json)
    {
        winrt::hstring type;
        JsonUtils::GetValueForKey(json, "type", type);
        if (type.empty())
        {
            auto terminalArgs = winrt::Microsoft::Terminal::Settings::Model::implementation::NewTerminalArgs::FromJson(json);
            // Don't let the user specify the __content property in their
            // settings. That's an internal-use-only property.
            if (terminalArgs.ContentId())
            {
                return { terminalArgs, { SettingsLoadWarnings::InvalidUseOfContent } };
            }
            return { terminalArgs, {} };
        }

        // For now, we don't support any other concrete types of content
        // with args. Just return a placeholder type that only includes the type
        return { *winrt::make_self<BaseContentArgs>(type), {} };
    }
    static Json::Value ContentArgsToJson(const Model::INewContentArgs& contentArgs)
    {
        if (contentArgs == nullptr)
        {
            return {};
        }
        // TerminalArgs don't have a type.
        if (contentArgs.Type().empty())
        {
            return winrt::Microsoft::Terminal::Settings::Model::implementation::NewTerminalArgs::ToJson(contentArgs.try_as<Model::NewTerminalArgs>());
        }

        // For now, we don't support any other concrete types of content
        // with args. Just return a placeholder.
        auto base{ winrt::make_self<BaseContentArgs>(contentArgs.Type()) };
        return BaseContentArgs::ToJson(*base);
    }

}

template<>
struct til::hash_trait<winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs>
{
    using M = winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs;
    using I = winrt::Microsoft::Terminal::Settings::Model::implementation::NewTerminalArgs;

    void operator()(hasher& h, const M& value) const noexcept
    {
        if (value)
        {
            winrt::get_self<I>(value)->Hash(h);
        }
    }
};
template<>
struct til::hash_trait<winrt::Microsoft::Terminal::Control::SelectionColor>
{
    using M = winrt::Microsoft::Terminal::Control::SelectionColor;

    void operator()(hasher& h, const M& value) const noexcept
    {
        if (value)
        {
            h.write(value.Color());
            h.write(value.IsIndex16());
        }
    }
};
template<>
struct til::hash_trait<winrt::Microsoft::Terminal::Settings::Model::INewContentArgs>
{
    using M = winrt::Microsoft::Terminal::Settings::Model::INewContentArgs;

    void operator()(hasher& h, const M& value) const noexcept
    {
        if (value)
        {
            h.write(value.Type());
            h.write(value.Hash());
        }
    }
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // New Tabs, Panes, and Windows all use NewTerminalArgs, which is more
    // complicated and doesn't play nice with the macro. So those we'll still
    // have to define manually.

    struct NewTabArgs : public NewTabArgsT<NewTabArgs>
    {
        NewTabArgs() = default;
        NewTabArgs(const Model::INewContentArgs& terminalArgs) :
            _ContentArgs{ terminalArgs } {};
        WINRT_PROPERTY(Model::INewContentArgs, ContentArgs, nullptr);

    public:
        hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NewTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_ContentArgs.Equals(_ContentArgs);
            }
            return false;
        }
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewTabArgs>();
            auto [content, warnings] = ContentArgsFromJson(json);
            args->_ContentArgs = content;
            return { *args, warnings };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<NewTabArgs>(val) };
            return ContentArgsToJson(args->_ContentArgs);
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewTabArgs>() };
            copy->_ContentArgs = _ContentArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            til::hasher h;
            h.write(ContentArgs());
            return h.finalize();
        }
        winrt::Windows::Foundation::Collections::IVectorView<ArgDescriptor> GetArgDescriptors()
        {
            if (_ContentArgs)
            {
                if (const auto contentArgs = _ContentArgs.try_as<IActionArgsDescriptorAccess>())
                {
                    return contentArgs.GetArgDescriptors();
                }
            }
            return {};
        }
        IInspectable GetArgAt(uint32_t index) const
        {
            if (_ContentArgs)
            {
                if (const auto contentArgs = _ContentArgs.try_as<IActionArgsDescriptorAccess>())
                {
                    return contentArgs.GetArgAt(index);
                }
            }
            return nullptr;
        }
        void SetArgAt(uint32_t index, IInspectable value)
        {
            if (_ContentArgs)
            {
                if (const auto contentArgs = _ContentArgs.try_as<IActionArgsDescriptorAccess>())
                {
                    contentArgs.SetArgAt(index, value);
                }
            }
        }
    };

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        SplitPaneArgs(SplitType splitMode, SplitDirection direction, float size, const Model::INewContentArgs& terminalArgs) :
            _SplitMode{ splitMode },
            _SplitDirection{ direction },
            _SplitSize{ size },
            _ContentArgs{ terminalArgs } {};
        SplitPaneArgs(SplitDirection direction, float size, const Model::INewContentArgs& terminalArgs) :
            _SplitDirection{ direction },
            _SplitSize{ size },
            _ContentArgs{ terminalArgs } {};
        SplitPaneArgs(SplitDirection direction, const Model::INewContentArgs& terminalArgs) :
            _SplitDirection{ direction },
            _ContentArgs{ terminalArgs } {};
        SplitPaneArgs(SplitType splitMode) :
            _SplitMode{ splitMode } {};

        SPLIT_PANE_ARGS(DECLARE_ARGS);
        WINRT_PROPERTY(Model::INewContentArgs, ContentArgs, nullptr);

    public:
        hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SplitPaneArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SplitDirection == _SplitDirection &&
                       (otherAsUs->_ContentArgs ? otherAsUs->_ContentArgs.Equals(_ContentArgs) :
                                                  otherAsUs->_ContentArgs == _ContentArgs) &&
                       otherAsUs->_SplitSize == _SplitSize &&
                       otherAsUs->_SplitMode == _SplitMode;
            }
            return false;
        }
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SplitPaneArgs>();
            JsonUtils::GetValueForKey(json, SplitDirectionKey, args->_SplitDirection);
            JsonUtils::GetValueForKey(json, SplitModeKey, args->_SplitMode);
            JsonUtils::GetValueForKey(json, SplitSizeKey, args->_SplitSize);
            if (args->SplitSize() >= 1 || args->SplitSize() <= 0)
            {
                return { nullptr, { SettingsLoadWarnings::InvalidSplitSize } };
            }

            auto [content, warnings] = ContentArgsFromJson(json);
            args->_ContentArgs = content;
            return { *args, warnings };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<SplitPaneArgs>(val) };
            auto json{ ContentArgsToJson(args->_ContentArgs) };
            JsonUtils::SetValueForKey(json, SplitDirectionKey, args->_SplitDirection);
            JsonUtils::SetValueForKey(json, SplitModeKey, args->_SplitMode);
            JsonUtils::SetValueForKey(json, SplitSizeKey, args->_SplitSize);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SplitPaneArgs>() };
            copy->_SplitDirection = _SplitDirection;
            copy->_ContentArgs = _ContentArgs.Copy();
            copy->_SplitMode = _SplitMode;
            copy->_SplitSize = _SplitSize;
            return *copy;
        }
        size_t Hash() const
        {
            til::hasher h;
            h.write(SplitDirection());
            h.write(ContentArgs());
            h.write(SplitMode());
            h.write(SplitSize());
            return h.finalize();
        }
        winrt::Windows::Foundation::Collections::IVectorView<Model::ArgDescriptor> GetArgDescriptors()
        {
            // Base args for SplitPane
            static const auto baseArgs = INIT_ARG_DESCRIPTORS(SPLIT_PANE_ARGS);

            // Cached merged variant: SplitPane + NewTerminalArgs
            static const auto mergedArgs = [] {
                std::vector<Model::ArgDescriptor> temp;

                // copy baseArgs
                for (const auto& desc : baseArgs)
                {
                    temp.push_back(desc);
                }

                // append NewTerminalArgs args
                const auto newTerminalArgsDesc{ Model::NewTerminalArgs{}.GetArgDescriptors() };
                for (const auto& desc : newTerminalArgsDesc)
                {
                    temp.push_back(desc);
                }

                return winrt::single_threaded_vector(std::move(temp)).GetView();
            }();

            // Pick which cached vector to return
            if (_ContentArgs && _ContentArgs.try_as<NewTerminalArgs>())
            {
                return mergedArgs;
            }
            return baseArgs;
        }
        IInspectable GetArgAt(uint32_t index)
        {
            const auto additionalArgCount = ARG_COUNT(SPLIT_PANE_ARGS);
            if (index < additionalArgCount)
            {
                X_MACRO_INDEX_BASE();
                SPLIT_PANE_ARGS(GET_ARG_BY_INDEX);
            }
            else
            {
                return _ContentArgs.as<IActionArgsDescriptorAccess>().GetArgAt(index - additionalArgCount);
            }
            return nullptr;
        }
        void SetArgAt(uint32_t index, IInspectable value)
        {
            const auto additionalArgCount = ARG_COUNT(SPLIT_PANE_ARGS);
            if (index < additionalArgCount)
            {
                X_MACRO_INDEX_BASE();
                SPLIT_PANE_ARGS(SET_ARG_BY_INDEX);
            }
            else
            {
                _ContentArgs.as<IActionArgsDescriptorAccess>().SetArgAt(index - additionalArgCount, value);
            }
        }
    };

    struct NewWindowArgs : public NewWindowArgsT<NewWindowArgs>
    {
        NewWindowArgs() = default;
        NewWindowArgs(const Model::INewContentArgs& terminalArgs) :
            _ContentArgs{ terminalArgs } {};
        WINRT_PROPERTY(Model::INewContentArgs, ContentArgs, nullptr);

    public:
        hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NewWindowArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_ContentArgs.Equals(_ContentArgs);
            }
            return false;
        }
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewWindowArgs>();
            auto [content, warnings] = ContentArgsFromJson(json);
            args->_ContentArgs = content;
            return { *args, warnings };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<NewWindowArgs>(val) };
            return ContentArgsToJson(args->_ContentArgs);
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewWindowArgs>() };
            copy->_ContentArgs = _ContentArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            til::hasher h;
            h.write(ContentArgs());
            return h.finalize();
        }
        winrt::Windows::Foundation::Collections::IVectorView<Model::ArgDescriptor> GetArgDescriptors()
        {
            return _ContentArgs.as<IActionArgsDescriptorAccess>().GetArgDescriptors();
        }
        IInspectable GetArgAt(uint32_t index) const
        {
            return _ContentArgs.as<IActionArgsDescriptorAccess>().GetArgAt(index);
        }
        void SetArgAt(uint32_t index, IInspectable value)
        {
            _ContentArgs.as<IActionArgsDescriptorAccess>().SetArgAt(index, value);
        }
    };

    ACTION_ARGS_STRUCT(CopyTextArgs, COPY_TEXT_ARGS);

    ACTION_ARGS_STRUCT(MovePaneArgs, MOVE_PANE_ARGS);

    ACTION_ARGS_STRUCT(SwitchToTabArgs, SWITCH_TO_TAB_ARGS);

    ACTION_ARGS_STRUCT(ResizePaneArgs, RESIZE_PANE_ARGS);

    ACTION_ARGS_STRUCT(MoveFocusArgs, MOVE_FOCUS_ARGS);

    ACTION_ARGS_STRUCT(SwapPaneArgs, SWAP_PANE_ARGS);

    ACTION_ARGS_STRUCT(AdjustFontSizeArgs, ADJUST_FONT_SIZE_ARGS);

    ACTION_ARGS_STRUCT(SendInputArgs, SEND_INPUT_ARGS);

    ACTION_ARGS_STRUCT(OpenSettingsArgs, OPEN_SETTINGS_ARGS);

    ACTION_ARGS_STRUCT(SetFocusModeArgs, SET_FOCUS_MODE_ARGS);

    ACTION_ARGS_STRUCT(SetFullScreenArgs, SET_FULL_SCREEN_ARGS);

    ACTION_ARGS_STRUCT(SetMaximizedArgs, SET_MAXIMIZED_ARGS);

    ACTION_ARGS_STRUCT(SetColorSchemeArgs, SET_COLOR_SCHEME_ARGS);

    ACTION_ARGS_STRUCT(SetTabColorArgs, SET_TAB_COLOR_ARGS);

    ACTION_ARGS_STRUCT(RenameTabArgs, RENAME_TAB_ARGS);

    ACTION_ARGS_STRUCT(ExecuteCommandlineArgs, EXECUTE_COMMANDLINE_ARGS);

    ACTION_ARGS_STRUCT(CloseOtherTabsArgs, CLOSE_OTHER_TABS_ARGS);

    ACTION_ARGS_STRUCT(CloseTabsAfterArgs, CLOSE_TABS_AFTER_ARGS);

    ACTION_ARGS_STRUCT(CloseTabArgs, CLOSE_TAB_ARGS);

    ACTION_ARGS_STRUCT(MoveTabArgs, MOVE_TAB_ARGS);

    ACTION_ARGS_STRUCT(ScrollUpArgs, SCROLL_UP_ARGS);

    ACTION_ARGS_STRUCT(ScrollDownArgs, SCROLL_DOWN_ARGS);

    ACTION_ARGS_STRUCT(ScrollToMarkArgs, SCROLL_TO_MARK_ARGS);

    ACTION_ARGS_STRUCT(AddMarkArgs, ADD_MARK_ARGS);

    ACTION_ARGS_STRUCT(ToggleCommandPaletteArgs, TOGGLE_COMMAND_PALETTE_ARGS);

    ACTION_ARGS_STRUCT(SaveSnippetArgs, SAVE_TASK_ARGS);

    ACTION_ARGS_STRUCT(SuggestionsArgs, SUGGESTIONS_ARGS);

    ACTION_ARGS_STRUCT(FindMatchArgs, FIND_MATCH_ARGS);

    ACTION_ARGS_STRUCT(PrevTabArgs, PREV_TAB_ARGS);

    ACTION_ARGS_STRUCT(NextTabArgs, NEXT_TAB_ARGS);

    ACTION_ARGS_STRUCT(RenameWindowArgs, RENAME_WINDOW_ARGS);

    ACTION_ARGS_STRUCT(SearchForTextArgs, SEARCH_FOR_TEXT_ARGS);

    struct GlobalSummonArgs : public GlobalSummonArgsT<GlobalSummonArgs>
    {
        ACTION_ARG_BODY(GlobalSummonArgs, GLOBAL_SUMMON_ARGS)
    public:
        // SPECIAL! This deserializer creates a GlobalSummonArgs with the
        // default values for quakeMode
        static FromJsonResult QuakeModeFromJson(const Json::Value& /*json*/)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<GlobalSummonArgs>();
            // We want to summon the window with the name "_quake" specifically.
            args->_Name = L"_quake";
            // We want the window to dropdown, with a 200ms duration.
            args->_DropdownDuration = 200;
            return { *args, {} };
        }
    };

    ACTION_ARGS_STRUCT(FocusPaneArgs, FOCUS_PANE_ARGS);

    ACTION_ARGS_STRUCT(ExportBufferArgs, EXPORT_BUFFER_ARGS);

    ACTION_ARGS_STRUCT(ClearBufferArgs, CLEAR_BUFFER_ARGS);

    struct MultipleActionsArgs : public MultipleActionsArgsT<MultipleActionsArgs>
    {
        MultipleActionsArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<ActionAndArgs>, Actions);
        static constexpr std::string_view ActionsKey{ "actions" };

    public:
        hstring GenerateName() const { return GenerateName(GetLibraryResourceLoader().ResourceContext()); }
        hstring GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext&) const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MultipleActionsArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Actions == _Actions;
            }
            return false;
        }
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MultipleActionsArgs>();
            JsonUtils::GetValueForKey(json, ActionsKey, args->_Actions);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };

            const auto args{ get_self<MultipleActionsArgs>(val) };
            JsonUtils::SetValueForKey(json, ActionsKey, args->_Actions);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<MultipleActionsArgs>() };
            copy->_Actions = _Actions;
            return *copy;
        }
        size_t Hash() const
        {
            til::hasher h;
            h.write(winrt::get_abi(_Actions));
            return h.finalize();
        }
        winrt::Windows::Foundation::Collections::IVectorView<Model::ArgDescriptor> GetArgDescriptors()
        {
            return {};
        }
        IInspectable GetArgAt(uint32_t /*index*/) const
        {
            return nullptr;
        }
        void SetArgAt(uint32_t /*index*/, IInspectable /*value*/)
        {
            throw winrt::hresult_not_implemented();
        }
    };

    ACTION_ARGS_STRUCT(AdjustOpacityArgs, ADJUST_OPACITY_ARGS);

    ACTION_ARGS_STRUCT(SelectCommandArgs, SELECT_COMMAND_ARGS);
    ACTION_ARGS_STRUCT(SelectOutputArgs, SELECT_OUTPUT_ARGS);

    ACTION_ARGS_STRUCT(ColorSelectionArgs, COLOR_SELECTION_ARGS);

}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
    BASIC_FACTORY(BaseContentArgs);
    BASIC_FACTORY(CopyTextArgs);
    BASIC_FACTORY(SwitchToTabArgs);
    BASIC_FACTORY(NewTerminalArgs);
    BASIC_FACTORY(NewTabArgs);
    BASIC_FACTORY(MoveFocusArgs);
    BASIC_FACTORY(MovePaneArgs);
    BASIC_FACTORY(SetTabColorArgs);
    BASIC_FACTORY(RenameTabArgs);
    BASIC_FACTORY(SwapPaneArgs);
    BASIC_FACTORY(SendInputArgs);
    BASIC_FACTORY(SplitPaneArgs);
    BASIC_FACTORY(SetFocusModeArgs);
    BASIC_FACTORY(SetFullScreenArgs);
    BASIC_FACTORY(SetMaximizedArgs);
    BASIC_FACTORY(SetColorSchemeArgs);
    BASIC_FACTORY(RenameWindowArgs);
    BASIC_FACTORY(ExecuteCommandlineArgs);
    BASIC_FACTORY(CloseOtherTabsArgs);
    BASIC_FACTORY(CloseTabsAfterArgs);
    BASIC_FACTORY(CloseTabArgs);
    BASIC_FACTORY(MoveTabArgs);
    BASIC_FACTORY(OpenSettingsArgs);
    BASIC_FACTORY(SaveSnippetArgs);
    BASIC_FACTORY(FindMatchArgs);
    BASIC_FACTORY(NewWindowArgs);
    BASIC_FACTORY(FocusPaneArgs);
    BASIC_FACTORY(PrevTabArgs);
    BASIC_FACTORY(NextTabArgs);
    BASIC_FACTORY(ExportBufferArgs);
    BASIC_FACTORY(ClearBufferArgs);
    BASIC_FACTORY(MultipleActionsArgs);
    BASIC_FACTORY(AdjustOpacityArgs);
    BASIC_FACTORY(SuggestionsArgs);
    BASIC_FACTORY(SelectCommandArgs);
    BASIC_FACTORY(SelectOutputArgs);
}

class ScopedResourceLoader;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    const ScopedResourceLoader& EnglishOnlyResourceLoader() noexcept;
}
