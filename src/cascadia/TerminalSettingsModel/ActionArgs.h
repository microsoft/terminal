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

#include "../../cascadia/inc/cppwinrt_utils.h"
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
            return other.Commandline() == _Commandline &&
                   other.StartingDirectory() == _StartingDirectory &&
                   other.TabTitle() == _TabTitle &&
                   other.TabColor() == _TabColor &&
                   other.ProfileIndex() == _ProfileIndex &&
                   other.Profile() == _Profile &&
                   other.SuppressApplicationTitle() == _SuppressApplicationTitle &&
                   other.ColorScheme() == _ColorScheme;
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

    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        ACTION_ARG(bool, SingleLine, false);
        ACTION_ARG(Windows::Foundation::IReference<Control::CopyFormat>, CopyFormatting, nullptr);

        static constexpr std::string_view SingleLineKey{ "singleLine" };
        static constexpr std::string_view CopyFormattingKey{ "copyFormatting" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<CopyTextArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SingleLine == _SingleLine &&
                       otherAsUs->_CopyFormatting == _CopyFormatting;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<CopyTextArgs>();
            JsonUtils::GetValueForKey(json, SingleLineKey, args->_SingleLine);
            JsonUtils::GetValueForKey(json, CopyFormattingKey, args->_CopyFormatting);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<CopyTextArgs>(val) };
            JsonUtils::SetValueForKey(json, SingleLineKey, args->_SingleLine);
            JsonUtils::SetValueForKey(json, CopyFormattingKey, args->_CopyFormatting);
            return json;
        }

        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CopyTextArgs>() };
            copy->_SingleLine = _SingleLine;
            copy->_CopyFormatting = _CopyFormatting;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SingleLine(), CopyFormatting());
        }
    };

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

    struct SwitchToTabArgs : public SwitchToTabArgsT<SwitchToTabArgs>
    {
        SwitchToTabArgs() = default;
        SwitchToTabArgs(uint32_t& tabIndex) :
            _TabIndex{ tabIndex } {};
        ACTION_ARG(uint32_t, TabIndex, 0);

        static constexpr std::string_view TabIndexKey{ "index" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SwitchToTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_TabIndex == _TabIndex;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SwitchToTabArgs>();
            JsonUtils::GetValueForKey(json, TabIndexKey, args->_TabIndex);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<SwitchToTabArgs>(val) };
            JsonUtils::SetValueForKey(json, TabIndexKey, args->_TabIndex);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SwitchToTabArgs>() };
            copy->_TabIndex = _TabIndex;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(TabIndex());
        }
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        ACTION_ARG(Model::ResizeDirection, ResizeDirection, ResizeDirection::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ResizePaneArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_ResizeDirection == _ResizeDirection;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ResizePaneArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_ResizeDirection);
            if (args->ResizeDirection() == ResizeDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<ResizePaneArgs>(val) };
            JsonUtils::SetValueForKey(json, DirectionKey, args->_ResizeDirection);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ResizePaneArgs>() };
            copy->_ResizeDirection = _ResizeDirection;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(ResizeDirection());
        }
    };

    struct MoveFocusArgs : public MoveFocusArgsT<MoveFocusArgs>
    {
        MoveFocusArgs() = default;
        MoveFocusArgs(Model::FocusDirection direction) :
            _FocusDirection{ direction } {};

        ACTION_ARG(Model::FocusDirection, FocusDirection, FocusDirection::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MoveFocusArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_FocusDirection == _FocusDirection;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MoveFocusArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_FocusDirection);
            if (args->FocusDirection() == FocusDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<MoveFocusArgs>(val) };
            JsonUtils::SetValueForKey(json, DirectionKey, args->_FocusDirection);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<MoveFocusArgs>() };
            copy->_FocusDirection = _FocusDirection;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(FocusDirection());
        }
    };

    struct AdjustFontSizeArgs : public AdjustFontSizeArgsT<AdjustFontSizeArgs>
    {
        AdjustFontSizeArgs() = default;
        ACTION_ARG(int32_t, Delta, 0);

        static constexpr std::string_view AdjustFontSizeDelta{ "delta" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<AdjustFontSizeArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Delta == _Delta;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<AdjustFontSizeArgs>();
            JsonUtils::GetValueForKey(json, AdjustFontSizeDelta, args->_Delta);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<AdjustFontSizeArgs>(val) };
            JsonUtils::SetValueForKey(json, AdjustFontSizeDelta, args->_Delta);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<AdjustFontSizeArgs>() };
            copy->_Delta = _Delta;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Delta());
        }
    };

    struct SendInputArgs : public SendInputArgsT<SendInputArgs>
    {
        SendInputArgs() = default;
        ACTION_ARG(winrt::hstring, Input, L"");

        static constexpr std::string_view InputKey{ "input" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            if (auto otherAsUs = other.try_as<SendInputArgs>(); otherAsUs)
            {
                return otherAsUs->_Input == _Input;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SendInputArgs>();
            JsonUtils::GetValueForKey(json, InputKey, args->_Input);
            if (args->Input().empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<SendInputArgs>(val) };
            JsonUtils::SetValueForKey(json, InputKey, args->_Input);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SendInputArgs>() };
            copy->_Input = _Input;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Input());
        }
    };

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        SplitPaneArgs(SplitType splitMode, SplitState style, double size, const Model::NewTerminalArgs& terminalArgs) :
            _SplitMode{ splitMode },
            _SplitStyle{ style },
            _SplitSize{ size },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitState style, double size, const Model::NewTerminalArgs& terminalArgs) :
            _SplitStyle{ style },
            _SplitSize{ size },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitState style, const Model::NewTerminalArgs& terminalArgs) :
            _SplitStyle{ style },
            _TerminalArgs{ terminalArgs } {};
        SplitPaneArgs(SplitType splitMode) :
            _SplitMode{ splitMode } {};
        ACTION_ARG(SplitState, SplitStyle, SplitState::Automatic);
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
                return otherAsUs->_SplitStyle == _SplitStyle &&
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
            JsonUtils::GetValueForKey(json, SplitKey, args->_SplitStyle);
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
            JsonUtils::SetValueForKey(json, SplitKey, args->_SplitStyle);
            JsonUtils::SetValueForKey(json, SplitModeKey, args->_SplitMode);
            JsonUtils::SetValueForKey(json, SplitSizeKey, args->_SplitSize);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SplitPaneArgs>() };
            copy->_SplitStyle = _SplitStyle;
            copy->_TerminalArgs = _TerminalArgs.Copy();
            copy->_SplitMode = _SplitMode;
            copy->_SplitSize = _SplitSize;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SplitStyle(), TerminalArgs(), SplitMode(), SplitSize());
        }
    };

    struct OpenSettingsArgs : public OpenSettingsArgsT<OpenSettingsArgs>
    {
        OpenSettingsArgs() = default;
        OpenSettingsArgs(const SettingsTarget& target) :
            _Target{ target } {}
        ACTION_ARG(SettingsTarget, Target, SettingsTarget::SettingsFile);

        static constexpr std::string_view TargetKey{ "target" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<OpenSettingsArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Target == _Target;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<OpenSettingsArgs>();
            JsonUtils::GetValueForKey(json, TargetKey, args->_Target);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<OpenSettingsArgs>(val) };
            JsonUtils::SetValueForKey(json, TargetKey, args->_Target);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<OpenSettingsArgs>() };
            copy->_Target = _Target;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Target());
        }
    };

    struct SetColorSchemeArgs : public SetColorSchemeArgsT<SetColorSchemeArgs>
    {
        SetColorSchemeArgs() = default;
        SetColorSchemeArgs(winrt::hstring name) :
            _SchemeName{ name } {};
        ACTION_ARG(winrt::hstring, SchemeName, L"");

        static constexpr std::string_view NameKey{ "colorScheme" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SetColorSchemeArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SchemeName == _SchemeName;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SetColorSchemeArgs>();
            JsonUtils::GetValueForKey(json, NameKey, args->_SchemeName);
            if (args->SchemeName().empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<SetColorSchemeArgs>(val) };
            JsonUtils::SetValueForKey(json, NameKey, args->_SchemeName);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SetColorSchemeArgs>() };
            copy->_SchemeName = _SchemeName;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SchemeName());
        }
    };

    struct SetTabColorArgs : public SetTabColorArgsT<SetTabColorArgs>
    {
        SetTabColorArgs() = default;
        ACTION_ARG(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, nullptr);

        static constexpr std::string_view ColorKey{ "color" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SetTabColorArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_TabColor == _TabColor;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SetTabColorArgs>();
            JsonUtils::GetValueForKey(json, ColorKey, args->_TabColor);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<SetTabColorArgs>(val) };
            JsonUtils::SetValueForKey(json, ColorKey, args->_TabColor);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SetTabColorArgs>() };
            copy->_TabColor = _TabColor;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(TabColor());
        }
    };

    struct RenameTabArgs : public RenameTabArgsT<RenameTabArgs>
    {
        RenameTabArgs() = default;
        ACTION_ARG(winrt::hstring, Title, L"");

        static constexpr std::string_view TitleKey{ "title" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<RenameTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Title == _Title;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<RenameTabArgs>();
            JsonUtils::GetValueForKey(json, TitleKey, args->_Title);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<RenameTabArgs>(val) };
            JsonUtils::SetValueForKey(json, TitleKey, args->_Title);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<RenameTabArgs>() };
            copy->_Title = _Title;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Title());
        }
    };

    struct ExecuteCommandlineArgs : public ExecuteCommandlineArgsT<ExecuteCommandlineArgs>
    {
        ExecuteCommandlineArgs() = default;
        ExecuteCommandlineArgs(winrt::hstring commandline) :
            _Commandline{ commandline } {};
        ACTION_ARG(winrt::hstring, Commandline, L"");

        static constexpr std::string_view CommandlineKey{ "commandline" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ExecuteCommandlineArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Commandline == _Commandline;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ExecuteCommandlineArgs>();
            JsonUtils::GetValueForKey(json, CommandlineKey, args->_Commandline);
            if (args->Commandline().empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<ExecuteCommandlineArgs>(val) };
            JsonUtils::SetValueForKey(json, CommandlineKey, args->_Commandline);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ExecuteCommandlineArgs>() };
            copy->_Commandline = _Commandline;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Commandline());
        }
    };

    struct CloseOtherTabsArgs : public CloseOtherTabsArgsT<CloseOtherTabsArgs>
    {
        CloseOtherTabsArgs() = default;
        CloseOtherTabsArgs(uint32_t& tabIndex) :
            _Index{ tabIndex } {};
        ACTION_ARG(Windows::Foundation::IReference<uint32_t>, Index, nullptr);

        static constexpr std::string_view IndexKey{ "index" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<CloseOtherTabsArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Index == _Index;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<CloseOtherTabsArgs>();
            JsonUtils::GetValueForKey(json, IndexKey, args->_Index);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<CloseOtherTabsArgs>(val) };
            JsonUtils::SetValueForKey(json, IndexKey, args->_Index);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CloseOtherTabsArgs>() };
            copy->_Index = _Index;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Index());
        }
    };

    struct CloseTabsAfterArgs : public CloseTabsAfterArgsT<CloseTabsAfterArgs>
    {
        CloseTabsAfterArgs() = default;
        CloseTabsAfterArgs(uint32_t& tabIndex) :
            _Index{ tabIndex } {};
        ACTION_ARG(Windows::Foundation::IReference<uint32_t>, Index, nullptr);

        static constexpr std::string_view IndexKey{ "index" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<CloseTabsAfterArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Index == _Index;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<CloseTabsAfterArgs>();
            JsonUtils::GetValueForKey(json, IndexKey, args->_Index);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<CloseTabsAfterArgs>(val) };
            JsonUtils::SetValueForKey(json, IndexKey, args->_Index);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CloseTabsAfterArgs>() };
            copy->_Index = _Index;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Index());
        }
    };

    struct MoveTabArgs : public MoveTabArgsT<MoveTabArgs>
    {
        MoveTabArgs() = default;
        MoveTabArgs(MoveTabDirection direction) :
            _Direction{ direction } {};
        ACTION_ARG(MoveTabDirection, Direction, MoveTabDirection::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MoveTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Direction == _Direction;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MoveTabArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_Direction);
            if (args->Direction() == MoveTabDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<MoveTabArgs>(val) };
            JsonUtils::SetValueForKey(json, DirectionKey, args->_Direction);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<MoveTabArgs>() };
            copy->_Direction = _Direction;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Direction());
        }
    };

    struct ScrollUpArgs : public ScrollUpArgsT<ScrollUpArgs>
    {
        ScrollUpArgs() = default;
        ACTION_ARG(Windows::Foundation::IReference<uint32_t>, RowsToScroll, nullptr);

        static constexpr std::string_view RowsToScrollKey{ "rowsToScroll" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ScrollUpArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_RowsToScroll == _RowsToScroll;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ScrollUpArgs>();
            JsonUtils::GetValueForKey(json, RowsToScrollKey, args->_RowsToScroll);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<ScrollUpArgs>(val) };
            JsonUtils::SetValueForKey(json, RowsToScrollKey, args->_RowsToScroll);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ScrollUpArgs>() };
            copy->_RowsToScroll = _RowsToScroll;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(RowsToScroll());
        }
    };

    struct ScrollDownArgs : public ScrollDownArgsT<ScrollDownArgs>
    {
        ScrollDownArgs() = default;
        ACTION_ARG(Windows::Foundation::IReference<uint32_t>, RowsToScroll, nullptr);

        static constexpr std::string_view RowsToScrollKey{ "rowsToScroll" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ScrollDownArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_RowsToScroll == _RowsToScroll;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ScrollDownArgs>();
            JsonUtils::GetValueForKey(json, RowsToScrollKey, args->_RowsToScroll);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<ScrollDownArgs>(val) };
            JsonUtils::SetValueForKey(json, RowsToScrollKey, args->_RowsToScroll);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ScrollDownArgs>() };
            copy->_RowsToScroll = _RowsToScroll;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(RowsToScroll());
        }
    };

    struct ToggleCommandPaletteArgs : public ToggleCommandPaletteArgsT<ToggleCommandPaletteArgs>
    {
        ToggleCommandPaletteArgs() = default;

        // To preserve backward compatibility the default is Action.
        ACTION_ARG(CommandPaletteLaunchMode, LaunchMode, CommandPaletteLaunchMode::Action);

        static constexpr std::string_view LaunchModeKey{ "launchMode" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ToggleCommandPaletteArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_LaunchMode == _LaunchMode;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ToggleCommandPaletteArgs>();
            JsonUtils::GetValueForKey(json, LaunchModeKey, args->_LaunchMode);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<ToggleCommandPaletteArgs>(val) };
            JsonUtils::SetValueForKey(json, LaunchModeKey, args->_LaunchMode);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ToggleCommandPaletteArgs>() };
            copy->_LaunchMode = _LaunchMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(LaunchMode());
        }
    };

    struct FindMatchArgs : public FindMatchArgsT<FindMatchArgs>
    {
        FindMatchArgs() = default;
        FindMatchArgs(FindMatchDirection direction) :
            _Direction{ direction } {};
        ACTION_ARG(FindMatchDirection, Direction, FindMatchDirection::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<FindMatchArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Direction == _Direction;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<FindMatchArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_Direction);
            if (args->Direction() == FindMatchDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<FindMatchArgs>(val) };
            JsonUtils::GetValueForKey(json, DirectionKey, args->_Direction);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<FindMatchArgs>() };
            copy->_Direction = _Direction;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Direction());
        }
    };

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

    struct PrevTabArgs : public PrevTabArgsT<PrevTabArgs>
    {
        PrevTabArgs() = default;
        ACTION_ARG(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, nullptr);
        static constexpr std::string_view SwitcherModeKey{ "tabSwitcherMode" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<PrevTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SwitcherMode == _SwitcherMode;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<PrevTabArgs>();
            JsonUtils::GetValueForKey(json, SwitcherModeKey, args->_SwitcherMode);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<PrevTabArgs>(val) };
            JsonUtils::SetValueForKey(json, SwitcherModeKey, args->_SwitcherMode);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<PrevTabArgs>() };
            copy->_SwitcherMode = _SwitcherMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SwitcherMode());
        }
    };

    struct NextTabArgs : public NextTabArgsT<NextTabArgs>
    {
        NextTabArgs() = default;
        ACTION_ARG(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, nullptr);
        static constexpr std::string_view SwitcherModeKey{ "tabSwitcherMode" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NextTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SwitcherMode == _SwitcherMode;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NextTabArgs>();
            JsonUtils::GetValueForKey(json, SwitcherModeKey, args->_SwitcherMode);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<NextTabArgs>(val) };
            JsonUtils::SetValueForKey(json, SwitcherModeKey, args->_SwitcherMode);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NextTabArgs>() };
            copy->_SwitcherMode = _SwitcherMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(SwitcherMode());
        }
    };

    struct RenameWindowArgs : public RenameWindowArgsT<RenameWindowArgs>
    {
        RenameWindowArgs() = default;
        ACTION_ARG(winrt::hstring, Name);
        static constexpr std::string_view NameKey{ "name" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<RenameWindowArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Name == _Name;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<RenameWindowArgs>();
            JsonUtils::GetValueForKey(json, NameKey, args->_Name);
            return { *args, {} };
        }
        static Json::Value ToJson(const IActionArgs& val)
        {
            if (!val)
            {
                return {};
            }
            Json::Value json{ Json::ValueType::objectValue };
            const auto args{ get_self<RenameWindowArgs>(val) };
            JsonUtils::SetValueForKey(json, NameKey, args->_Name);
            return json;
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<RenameWindowArgs>() };
            copy->_Name = _Name;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(Name());
        }
    };

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
            JsonUtils::GetValueForKey(json, DropdownDurationKey, args->_DropdownDuration);
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

}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
    BASIC_FACTORY(SwitchToTabArgs);
    BASIC_FACTORY(NewTerminalArgs);
    BASIC_FACTORY(NewTabArgs);
    BASIC_FACTORY(MoveFocusArgs);
    BASIC_FACTORY(SplitPaneArgs);
    BASIC_FACTORY(SetColorSchemeArgs);
    BASIC_FACTORY(ExecuteCommandlineArgs);
    BASIC_FACTORY(CloseOtherTabsArgs);
    BASIC_FACTORY(CloseTabsAfterArgs);
    BASIC_FACTORY(MoveTabArgs);
    BASIC_FACTORY(OpenSettingsArgs);
    BASIC_FACTORY(FindMatchArgs);
    BASIC_FACTORY(NewWindowArgs);
}
