// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// HEY YOU: When adding ActionArgs types, make sure to add the corresponding
//          *.g.cpp to ActionArgs.cpp!
#include "ActionEventArgs.g.h"
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
#include "SetColorSchemeArgs.g.h"
#include "SetTabColorArgs.g.h"
#include "RenameTabArgs.g.h"
#include "ExecuteCommandlineArgs.g.h"
#include "CloseOtherTabsArgs.g.h"
#include "CloseTabsAfterArgs.g.h"
#include "CloseTabArgs.g.h"
#include "ScrollUpArgs.g.h"
#include "ScrollDownArgs.g.h"
#include "MoveTabArgs.g.h"
#include "ToggleCommandPaletteArgs.g.h"
#include "FindMatchArgs.g.h"
#include "NewWindowArgs.g.h"
#include "PrevTabArgs.g.h"
#include "NextTabArgs.g.h"
#include "RenameWindowArgs.g.h"
#include "GlobalSummonArgs.g.h"
#include "FocusPaneArgs.g.h"
#include "ClearBufferArgs.g.h"
#include "MultipleActionsArgs.g.h"

#include "JsonUtils.h"
#include "HashUtils.h"
#include "TerminalWarnings.h"
#include "../inc/WindowingBehavior.h"

#include "TerminalSettingsSerializationHelpers.h"

#define ACTION_ARG(type, name, ...)                                                                    \
public:                                                                                                \
    type name() const noexcept { return _##name.has_value() ? _##name.value() : type{ __VA_ARGS__ }; } \
    void name(const type& value) noexcept { _##name = value; }                                         \
                                                                                                       \
private:                                                                                               \
    std::optional<type> _##name{ std::nullopt };

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as an ActionArgs
//   class that implements IActionArgs
// * ActionEventArgs holds a single IActionArgs. For events that don't need
//   additional args, this can be nullptr.

template<>
constexpr size_t Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(const winrt::Microsoft::Terminal::Settings::Model::IActionArgs& args)
{
    return gsl::narrow_cast<size_t>(args.Hash());
}

template<>
constexpr size_t Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& args)
{
    return gsl::narrow_cast<size_t>(args.Hash());
}

// Retrieves the hash value for an empty-constructed object.
template<typename T>
static size_t EmptyHash()
{
    // cache the value of the empty hash
    static const size_t cachedHash = winrt::make_self<T>()->Hash();
    return cachedHash;
}

////////////////////////////////////////////////////////////////////////////////
// BEGIN XMACRO MADNESS
////////////////////////////////////////////////////////////////////////////////

// A helper for saying "This class has no other property validation". This will
// make sense below.
#define NO_OTHER_VALIDATION() ;

// Below are definitions for the properties for each and every ActionArgs type.
// Each should maintain the same format:
//
// #define MY_FOO_ARGS(X)                    \
//     X(ParamOneType, One, "one", {default args}) \
//     X(Windows::Foundation::IReference<ParamTwoType>, Two, "two", nullptr)
//     { etc... }
//
// If one of your properties needs some additional validation done to it, then
// define a VALIDATE_MY_FOO() macro with the validation, and include that when
// calling ACTION_ARGS_STRUCT

////////////////////////////////////////////////////////////////////////////////
#define COPY_TEXT_ARGS(X)                    \
    X(bool, SingleLine, "singleLine", false) \
    X(Windows::Foundation::IReference<Control::CopyFormat>, CopyFormatting, "copyFormatting", nullptr)

////////////////////////////////////////////////////////////////////////////////
#define MOVE_PANE_ARGS(X) \
    X(uint32_t, TabIndex, "index", 0)

////////////////////////////////////////////////////////////////////////////////
#define SWITCH_TO_TAB_ARGS(X) \
    X(uint32_t, TabIndex, "index", 0)

////////////////////////////////////////////////////////////////////////////////
#define RESIZE_PANE_ARGS(X) \
    X(Model::ResizeDirection, ResizeDirection, "direction", Model::ResizeDirection::None)

#define VALIDATE_RESIZE_PANE()                                                  \
    if (args->ResizeDirection() == ResizeDirection::None)                       \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define MOVE_FOCUS_ARGS(X) \
    X(Model::FocusDirection, FocusDirection, "direction", Model::FocusDirection::None)

#define VALIDATE_MOVE_FOCUS()                                                   \
    if (args->FocusDirection() == Model::FocusDirection::None)                  \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define SWAP_PANE_ARGS(X) \
    X(Model::FocusDirection, Direction, "direction", Model::FocusDirection::None)

#define VALIDATE_SWAP_PANE()                                                    \
    if (args->Direction() == Model::FocusDirection::None)                       \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define ADJUST_FONT_SIZE_ARGS(X) \
    X(int32_t, Delta, "delta", 0)

////////////////////////////////////////////////////////////////////////////////
#define SEND_INPUT_ARGS(X) \
    X(winrt::hstring, Input, "input", L"")

#define VALIDATE_SEND_INPUT()                                                   \
    if (args->Input().empty())                                                  \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define OPEN_SETTINGS_ARGS(X) \
    X(SettingsTarget, Target, "target", SettingsTarget::SettingsFile)

////////////////////////////////////////////////////////////////////////////////
#define SET_COLOR_SCHEME_ARGS(X) \
    X(winrt::hstring, SchemeName, "colorScheme", L"")

#define VALIDATE_SET_COLOR_SCHEME()                                             \
    if (args->SchemeName().empty())                                             \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define SET_TAB_COLOR_ARGS(X) \
    X(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, "color", nullptr)

////////////////////////////////////////////////////////////////////////////////
#define RENAME_TAB_ARGS(X) \
    X(winrt::hstring, Title, "title", L"")

////////////////////////////////////////////////////////////////////////////////
#define EXECUTE_COMMANDLINE_ARGS(X) \
    X(winrt::hstring, Commandline, "commandline", L"")

#define VALIDATE_EXECUTE_COMMANDLINE()                                          \
    if (args->Commandline().empty())                                            \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define CLOSE_OTHER_TABS_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", nullptr)

#define VALIDATE_CLOSE_OTHER_TABS() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define CLOSE_TABS_AFTER_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", nullptr)

#define VALIDATE_CLOSE_TABS_AFTER() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define CLOSE_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, Index, "index", nullptr)

#define VALIDATE_CLOSE_TAB() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define MOVE_TAB_ARGS(X) \
    X(MoveTabDirection, Direction, "direction", MoveTabDirection::None)

#define VALIDATE_MOVE_TAB()                                                     \
    if (args->Direction() == MoveTabDirection::None)                            \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define SCROLL_UP_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, RowsToScroll, "rowsToScroll", nullptr)

#define VALIDATE_SCROLL_UP() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define SCROLL_DOWN_ARGS(X) \
    X(Windows::Foundation::IReference<uint32_t>, RowsToScroll, "rowsToScroll", nullptr)

#define VALIDATE_SCROLL_DOWN() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define TOGGLE_COMMAND_PALETTE_ARGS(X) \
    X(CommandPaletteLaunchMode, LaunchMode, "launchMode", CommandPaletteLaunchMode::Action)

#define VALIDATE_TOGGLE_COMMAND_PALETTE() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define FIND_MATCH_ARGS(X) \
    X(FindMatchDirection, Direction, "direction", FindMatchDirection::None)

#define VALIDATE_FIND_MATCH()                                                   \
    if (args->Direction() == FindMatchDirection::None)                          \
    {                                                                           \
        return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } }; \
    }
////////////////////////////////////////////////////////////////////////////////
#define PREV_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, "tabSwitcherMode", nullptr)

#define VALIDATE_PREV_TAB() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define NEXT_TAB_ARGS(X) \
    X(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, "tabSwitcherMode", nullptr)

#define VALIDATE_NEXT_TAB() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define RENAME_WINDOW_ARGS(X) \
    X(winrt::hstring, Name, "name", L"")

#define VALIDATE_RENAME_WINDOW() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////
#define FOCUS_PANE_ARGS(X) \
    X(uint32_t, Id, "id", 0u)

#define VALIDATE_FOCUS_PANE NO_OTHER_VALIDATION
////////////////////////////////////////////////////////////////////////////////
#define CLEAR_BUFFER_ARGS(X) \
    X(winrt::Microsoft::Terminal::Control::ClearBufferType, Clear, "clear", winrt::Microsoft::Terminal::Control::ClearBufferType::All)

#define VALIDATE_CLEAR_BUFFER() NO_OTHER_VALIDATION()
////////////////////////////////////////////////////////////////////////////////

// MACRO HACKS
//
// We want to have code that looks like:
//
// FooArgs(const ParamOne& one, const ParamTwo& two) :
//     _One{ one }, _Two{ two } {};
//
// However, if we just use the x-macro for this straight up, then the list will
// have a trailing comma at the end of it, and won't compile. So, we're creating
// this placeholder size-0 struct. It's going to be the last param for all the
// args' ctors. It'll have a default value, so no one will need to know about
// it. This will let us use the macro to populate the ctors as well.

struct InitListPlaceholder
{
};
////////////////////////////////////////////////////////////////////////////////
//
// The complete ActionAndArgs definition. Each macro above BEGIN_ACTION_ARG is
// some element of the class definition that will use the x-macro.
//
// You'll author a new arg by:
//   1: define a new x-macro above with all it's properties
//   2: If needed, add extra property validation in a new VALIDATE_MY_FOO macro
//   3. Define the class with:
//
//   ACTION_ARGS_STRUCT(MyFooArgs, MY_FOO_ARGS, VALIDATE_MY_FOO);
//
// In that macro, we'll use the passed-in macro (MY_FOO_ARGS) with each of these
// macros below, which will generate the various parts of the class body.
//

// Property definitions, and JSON keys
#define DECLARE_ARGS(type, name, jsonKey, ...)              \
    static constexpr std::string_view name##Key{ jsonKey }; \
    ACTION_ARG(type, name, ##__VA_ARGS__);

// Parameters to the non-default ctor
#define CTOR_PARAMS(type, name, jsonKey, ...) \
    const type &name##Param,

// initializers in the ctor
#define CTOR_INITS(type, name, jsonKey, ...) \
    _##name{ name##Param },

// check each property in the Equals() method
#define EQUALS_ARGS(type, name, jsonKey, ...) \
    &&(otherAsUs->_##name == _##name)

// JSON deserialization
#define FROM_JSON_ARGS(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, args->_##name);

// JSON serialization
#define TO_JSON_ARGS(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, args->_##name);

// Copy each property in the Copy() method
#define COPY_ARGS(type, name, jsonKey, ...) \
    copy->_##name = _##name;

// hash each property in Hash()
#define HASH_ARGS(type, name, jsonKey, ...) \
    , name()

// Use ACTION_ARGS_STRUCT when you've got no other customizing to do.
#define ACTION_ARGS_STRUCT(className, argsMacro, otherValidation) \
    BEGIN_ACTION_ARG(className, argsMacro, otherValidation)       \
    END_ACTION_ARG()

#define BEGIN_ACTION_ARG(className, argsMacro, otherValidation)                     \
    struct className : public className##T<className>                               \
    {                                                                               \
        className() = default;                                                      \
        className(                                                                  \
            argsMacro(CTOR_PARAMS) InitListPlaceholder = {}) :                      \
            argsMacro(CTOR_INITS) _placeholder{} {};                                \
        argsMacro(DECLARE_ARGS);                                                    \
                                                                                    \
    private:                                                                        \
        InitListPlaceholder _placeholder;                                           \
                                                                                    \
    public:                                                                         \
        hstring GenerateName() const;                                               \
        bool Equals(const IActionArgs& other)                                       \
        {                                                                           \
            auto otherAsUs = other.try_as<className>();                             \
            if (otherAsUs)                                                          \
            {                                                                       \
                return true argsMacro(EQUALS_ARGS);                                 \
            }                                                                       \
            return false;                                                           \
        };                                                                          \
        static FromJsonResult FromJson(const Json::Value& json)                     \
        {                                                                           \
            auto args = winrt::make_self<className>();                              \
            argsMacro(FROM_JSON_ARGS);                                              \
            otherValidation();                                                      \
            return { *args, {} };                                                   \
        }                                                                           \
        static Json::Value ToJson(const IActionArgs& val)                           \
        {                                                                           \
            if (!val)                                                               \
            {                                                                       \
                return {};                                                          \
            }                                                                       \
            Json::Value json{ Json::ValueType::objectValue };                       \
            const auto args{ get_self<className>(val) };                            \
            argsMacro(TO_JSON_ARGS);                                                \
            return json;                                                            \
        }                                                                           \
        IActionArgs Copy() const                                                    \
        {                                                                           \
            auto copy{ winrt::make_self<className>() };                             \
            argsMacro(COPY_ARGS);                                                   \
            return *copy;                                                           \
        }                                                                           \
        size_t Hash() const                                                         \
        {                                                                           \
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty( \
                0 argsMacro(HASH_ARGS));                                            \
        }

#define END_ACTION_ARG() \
    }                    \
    ;

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

    struct NewTerminalArgs : public NewTerminalArgsT<NewTerminalArgs>
    {
        NewTerminalArgs() = default;
        NewTerminalArgs(int32_t& profileIndex) :
            _ProfileIndex{ profileIndex } {};
        ACTION_ARG(winrt::hstring, Commandline, L"");
        ACTION_ARG(winrt::hstring, StartingDirectory, L"");
        ACTION_ARG(winrt::hstring, TabTitle, L"");
        ACTION_ARG(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, nullptr);
        ACTION_ARG(Windows::Foundation::IReference<int32_t>, ProfileIndex, nullptr);
        ACTION_ARG(winrt::hstring, Profile, L"");
        ACTION_ARG(Windows::Foundation::IReference<bool>, SuppressApplicationTitle, nullptr);
        ACTION_ARG(winrt::hstring, ColorScheme);

        static constexpr std::string_view CommandlineKey{ "commandline" };
        static constexpr std::string_view StartingDirectoryKey{ "startingDirectory" };
        static constexpr std::string_view TabTitleKey{ "tabTitle" };
        static constexpr std::string_view TabColorKey{ "tabColor" };
        static constexpr std::string_view ProfileIndexKey{ "index" };
        static constexpr std::string_view ProfileKey{ "profile" };
        static constexpr std::string_view SuppressApplicationTitleKey{ "suppressApplicationTitle" };
        static constexpr std::string_view ColorSchemeKey{ "colorScheme" };

    public:
        hstring GenerateName() const;
        hstring ToCommandline() const;

        bool Equals(const Model::NewTerminalArgs& other)
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
                       otherAsUs->_SuppressApplicationTitle == _SuppressApplicationTitle &&
                       otherAsUs->_ColorScheme == _ColorScheme;
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
            JsonUtils::GetValueForKey(json, TabColorKey, args->_TabColor);
            JsonUtils::GetValueForKey(json, SuppressApplicationTitleKey, args->_SuppressApplicationTitle);
            JsonUtils::GetValueForKey(json, ColorSchemeKey, args->_ColorScheme);
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
            JsonUtils::SetValueForKey(json, TabColorKey, args->_TabColor);
            JsonUtils::SetValueForKey(json, SuppressApplicationTitleKey, args->_SuppressApplicationTitle);
            JsonUtils::SetValueForKey(json, ColorSchemeKey, args->_ColorScheme);
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
            copy->_SuppressApplicationTitle = _SuppressApplicationTitle;
            copy->_ColorScheme = _ColorScheme;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Commandline(), StartingDirectory(), TabTitle(), TabColor(), ProfileIndex(), Profile(), SuppressApplicationTitle(), ColorScheme());
        }
    };

    ACTION_ARGS_STRUCT(CopyTextArgs, COPY_TEXT_ARGS, NO_OTHER_VALIDATION);

    struct NewTabArgs : public NewTabArgsT<NewTabArgs>
    {
        NewTabArgs() = default;
        NewTabArgs(const Model::NewTerminalArgs& terminalArgs) :
            _TerminalArgs{ terminalArgs } {};
        WINRT_PROPERTY(Model::NewTerminalArgs, TerminalArgs, nullptr);

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NewTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_TerminalArgs.Equals(_TerminalArgs);
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewTabArgs>();
            args->_TerminalArgs = NewTerminalArgs::FromJson(json);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<NewTabArgs>(val) };
            return NewTerminalArgs::ToJson(args->_TerminalArgs);
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewTabArgs>() };
            copy->_TerminalArgs = _TerminalArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(TerminalArgs());
        }
    };

    ACTION_ARGS_STRUCT(MovePaneArgs, MOVE_PANE_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(SwitchToTabArgs, SWITCH_TO_TAB_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(ResizePaneArgs, RESIZE_PANE_ARGS, VALIDATE_RESIZE_PANE);

    ACTION_ARGS_STRUCT(MoveFocusArgs, MOVE_FOCUS_ARGS, VALIDATE_MOVE_FOCUS);

    ACTION_ARGS_STRUCT(SwapPaneArgs, SWAP_PANE_ARGS, VALIDATE_SWAP_PANE);

    ACTION_ARGS_STRUCT(AdjustFontSizeArgs, ADJUST_FONT_SIZE_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(SendInputArgs, SEND_INPUT_ARGS, VALIDATE_SEND_INPUT);

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        SplitPaneArgs(SplitType splitMode, SplitDirection direction, double size, const Model::NewTerminalArgs& terminalArgs) :
            _SplitMode{ splitMode },
            _SplitDirection{ direction },
            _SplitSize{ size },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitDirection direction, double size, const Model::NewTerminalArgs& terminalArgs) :
            _SplitDirection{ direction },
            _SplitSize{ size },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitDirection direction, const Model::NewTerminalArgs& terminalArgs) :
            _SplitDirection{ direction },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitType splitMode) :
            _SplitMode{ splitMode } {};
        ACTION_ARG(Model::SplitDirection, SplitDirection, SplitDirection::Automatic);
        WINRT_PROPERTY(Model::NewTerminalArgs, TerminalArgs, nullptr);
        ACTION_ARG(SplitType, SplitMode, SplitType::Manual);
        ACTION_ARG(double, SplitSize, .5);

        static constexpr std::string_view SplitKey{ "split" };
        static constexpr std::string_view SplitModeKey{ "splitMode" };
        static constexpr std::string_view SplitSizeKey{ "size" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SplitPaneArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SplitDirection == _SplitDirection &&
                       (otherAsUs->_TerminalArgs ? otherAsUs->_TerminalArgs.Equals(_TerminalArgs) :
                                                   otherAsUs->_TerminalArgs == _TerminalArgs) &&
                       otherAsUs->_SplitSize == _SplitSize &&
                       otherAsUs->_SplitMode == _SplitMode;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SplitPaneArgs>();
            args->_TerminalArgs = NewTerminalArgs::FromJson(json);
            JsonUtils::GetValueForKey(json, SplitKey, args->_SplitDirection);
            JsonUtils::GetValueForKey(json, SplitModeKey, args->_SplitMode);
            JsonUtils::GetValueForKey(json, SplitSizeKey, args->_SplitSize);
            if (args->SplitSize() >= 1 || args->SplitSize() <= 0)
            {
                return { nullptr, { SettingsLoadWarnings::InvalidSplitSize } };
            }
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<SplitPaneArgs>(val) };
            auto json{ NewTerminalArgs::ToJson(args->_TerminalArgs) };
            JsonUtils::SetValueForKey(json, SplitKey, args->_SplitDirection);
            JsonUtils::SetValueForKey(json, SplitModeKey, args->_SplitMode);
            JsonUtils::SetValueForKey(json, SplitSizeKey, args->_SplitSize);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SplitPaneArgs>() };
            copy->_SplitDirection = _SplitDirection;
            copy->_TerminalArgs = _TerminalArgs.Copy();
            copy->_SplitMode = _SplitMode;
            copy->_SplitSize = _SplitSize;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SplitDirection(), TerminalArgs(), SplitMode(), SplitSize());
        }
    };

    ACTION_ARGS_STRUCT(OpenSettingsArgs, OPEN_SETTINGS_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(SetColorSchemeArgs, SET_COLOR_SCHEME_ARGS, VALIDATE_SET_COLOR_SCHEME);

    ACTION_ARGS_STRUCT(SetTabColorArgs, SET_TAB_COLOR_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(RenameTabArgs, RENAME_TAB_ARGS, NO_OTHER_VALIDATION);

    ACTION_ARGS_STRUCT(ExecuteCommandlineArgs, EXECUTE_COMMANDLINE_ARGS, VALIDATE_EXECUTE_COMMANDLINE);

    ACTION_ARGS_STRUCT(CloseOtherTabsArgs, CLOSE_OTHER_TABS_ARGS, VALIDATE_CLOSE_OTHER_TABS);

    ACTION_ARGS_STRUCT(CloseTabsAfterArgs, CLOSE_TABS_AFTER_ARGS, VALIDATE_CLOSE_TABS_AFTER);

    ACTION_ARGS_STRUCT(CloseTabArgs, CLOSE_TAB_ARGS, VALIDATE_CLOSE_TAB);

    ACTION_ARGS_STRUCT(MoveTabArgs, MOVE_TAB_ARGS, VALIDATE_MOVE_TAB);

    ACTION_ARGS_STRUCT(ScrollUpArgs, SCROLL_UP_ARGS, VALIDATE_SCROLL_UP);

    ACTION_ARGS_STRUCT(ScrollDownArgs, SCROLL_DOWN_ARGS, VALIDATE_SCROLL_DOWN);

    ACTION_ARGS_STRUCT(ToggleCommandPaletteArgs, TOGGLE_COMMAND_PALETTE_ARGS, VALIDATE_TOGGLE_COMMAND_PALETTE);

    ACTION_ARGS_STRUCT(FindMatchArgs, FIND_MATCH_ARGS, VALIDATE_FIND_MATCH);

    struct NewWindowArgs : public NewWindowArgsT<NewWindowArgs>
    {
        NewWindowArgs() = default;
        NewWindowArgs(const Model::NewTerminalArgs& terminalArgs) :
            _TerminalArgs{ terminalArgs } {};
        WINRT_PROPERTY(Model::NewTerminalArgs, TerminalArgs, nullptr);

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NewWindowArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_TerminalArgs.Equals(_TerminalArgs);
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewWindowArgs>();
            args->_TerminalArgs = NewTerminalArgs::FromJson(json);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            const auto args{ get_self<NewWindowArgs>(val) };
            return NewTerminalArgs::ToJson(args->_TerminalArgs);
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewWindowArgs>() };
            copy->_TerminalArgs = _TerminalArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(TerminalArgs());
        }
    };

    ACTION_ARGS_STRUCT(PrevTabArgs, PREV_TAB_ARGS, VALIDATE_PREV_TAB);

    ACTION_ARGS_STRUCT(NextTabArgs, NEXT_TAB_ARGS, VALIDATE_NEXT_TAB);

    ACTION_ARGS_STRUCT(RenameWindowArgs, RENAME_WINDOW_ARGS, VALIDATE_RENAME_WINDOW);

    struct GlobalSummonArgs : public GlobalSummonArgsT<GlobalSummonArgs>
    {
        GlobalSummonArgs() = default;
        ACTION_ARG(winrt::hstring, Name, L"");
        ACTION_ARG(Model::DesktopBehavior, Desktop, Model::DesktopBehavior::ToCurrent);
        ACTION_ARG(Model::MonitorBehavior, Monitor, Model::MonitorBehavior::ToMouse);
        ACTION_ARG(bool, ToggleVisibility, true);
        ACTION_ARG(uint32_t, DropdownDuration, 0);

        static constexpr std::string_view NameKey{ "name" };
        static constexpr std::string_view DesktopKey{ "desktop" };
        static constexpr std::string_view MonitorKey{ "monitor" };
        static constexpr std::string_view ToggleVisibilityKey{ "toggleVisibility" };
        static constexpr std::string_view DropdownDurationKey{ "dropdownDuration" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            if (auto otherAsUs = other.try_as<GlobalSummonArgs>())
            {
                return otherAsUs->_Name == _Name &&
                       otherAsUs->_Desktop == _Desktop &&
                       otherAsUs->_Monitor == _Monitor &&
                       otherAsUs->_DropdownDuration == _DropdownDuration &&
                       otherAsUs->_ToggleVisibility == _ToggleVisibility;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<GlobalSummonArgs>();
            JsonUtils::GetValueForKey(json, NameKey, args->_Name);
            JsonUtils::GetValueForKey(json, DesktopKey, args->_Desktop);
            JsonUtils::GetValueForKey(json, MonitorKey, args->_Monitor);
            JsonUtils::GetValueForKey(json, DropdownDurationKey, args->_DropdownDuration);
            JsonUtils::GetValueForKey(json, ToggleVisibilityKey, args->_ToggleVisibility);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<GlobalSummonArgs>(val) };
            JsonUtils::SetValueForKey(json, NameKey, args->_Name);
            JsonUtils::SetValueForKey(json, DesktopKey, args->_Desktop);
            JsonUtils::SetValueForKey(json, MonitorKey, args->_Monitor);
            JsonUtils::SetValueForKey(json, DropdownDurationKey, args->_DropdownDuration);
            JsonUtils::SetValueForKey(json, ToggleVisibilityKey, args->_ToggleVisibility);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<GlobalSummonArgs>() };
            copy->_Name = _Name;
            copy->_Desktop = _Desktop;
            copy->_Monitor = _Monitor;
            copy->_DropdownDuration = _DropdownDuration;
            copy->_ToggleVisibility = _ToggleVisibility;
            return *copy;
        }
        // SPECIAL! This deserializer creates a GlobalSummonArgs with the
        // default values for quakeMode
        static FromJsonResult QuakeModeFromJson(const Json::Value& /*json*/)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<GlobalSummonArgs>();
            // We want to summon the window with the name "_quake" specifically.
            args->_Name = QuakeWindowName;
            // We want the window to dropdown, with a 200ms duration.
            args->_DropdownDuration = 200;
            return { *args, {} };
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Name(), Desktop(), Monitor(), DropdownDuration(), ToggleVisibility());
        }
    };

    ACTION_ARGS_STRUCT(FocusPaneArgs, FOCUS_PANE_ARGS, VALIDATE_FOCUS_PANE);

    ACTION_ARGS_STRUCT(ClearBufferArgs, CLEAR_BUFFER_ARGS, VALIDATE_CLEAR_BUFFER);

    struct MultipleActionsArgs : public MultipleActionsArgsT<MultipleActionsArgs>
    {
        MultipleActionsArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<ActionAndArgs>, Actions);
        static constexpr std::string_view ActionsKey{ "actions" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MultipleActionsArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Actions == _Actions;
            }
            return false;
        };
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
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Actions);
        }
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
    BASIC_FACTORY(SwitchToTabArgs);
    BASIC_FACTORY(NewTerminalArgs);
    BASIC_FACTORY(NewTabArgs);
    BASIC_FACTORY(MoveFocusArgs);
    BASIC_FACTORY(MovePaneArgs);
    BASIC_FACTORY(SetTabColorArgs);
    BASIC_FACTORY(SwapPaneArgs);
    BASIC_FACTORY(SplitPaneArgs);
    BASIC_FACTORY(SetColorSchemeArgs);
    BASIC_FACTORY(RenameWindowArgs);
    BASIC_FACTORY(ExecuteCommandlineArgs);
    BASIC_FACTORY(CloseOtherTabsArgs);
    BASIC_FACTORY(CloseTabsAfterArgs);
    BASIC_FACTORY(CloseTabArgs);
    BASIC_FACTORY(MoveTabArgs);
    BASIC_FACTORY(OpenSettingsArgs);
    BASIC_FACTORY(FindMatchArgs);
    BASIC_FACTORY(NewWindowArgs);
    BASIC_FACTORY(FocusPaneArgs);
    BASIC_FACTORY(PrevTabArgs);
    BASIC_FACTORY(NextTabArgs);
    BASIC_FACTORY(ClearBufferArgs);
    BASIC_FACTORY(MultipleActionsArgs);
}
