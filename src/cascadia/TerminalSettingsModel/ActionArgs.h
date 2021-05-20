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
        WINRT_PROPERTY(winrt::hstring, Commandline, L"");
        WINRT_PROPERTY(winrt::hstring, StartingDirectory, L"");
        WINRT_PROPERTY(winrt::hstring, TabTitle, L"");
        WINRT_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, nullptr);
        WINRT_PROPERTY(Windows::Foundation::IReference<int32_t>, ProfileIndex, nullptr);
        WINRT_PROPERTY(winrt::hstring, Profile, L"");
        WINRT_PROPERTY(Windows::Foundation::IReference<bool>, SuppressApplicationTitle, nullptr);
        WINRT_PROPERTY(winrt::hstring, ColorScheme);

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
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Commandline, _StartingDirectory, _TabTitle, _TabColor, _ProfileIndex, _Profile, _SuppressApplicationTitle, _ColorScheme);
        }
    };

    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        WINRT_PROPERTY(bool, SingleLine, false);
        WINRT_PROPERTY(Windows::Foundation::IReference<Control::CopyFormat>, CopyFormatting, nullptr);

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

        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CopyTextArgs>() };
            copy->_SingleLine = _SingleLine;
            copy->_CopyFormatting = _CopyFormatting;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_SingleLine, _CopyFormatting);
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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewTabArgs>() };
            copy->_TerminalArgs = _TerminalArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_TerminalArgs);
        }
    };

    struct SwitchToTabArgs : public SwitchToTabArgsT<SwitchToTabArgs>
    {
        SwitchToTabArgs() = default;
        SwitchToTabArgs(uint32_t& tabIndex) :
            _TabIndex{ tabIndex } {};
        WINRT_PROPERTY(uint32_t, TabIndex, 0);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SwitchToTabArgs>() };
            copy->_TabIndex = _TabIndex;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_TabIndex);
        }
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        WINRT_PROPERTY(Model::ResizeDirection, ResizeDirection, ResizeDirection::None);

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
            if (args->_ResizeDirection == ResizeDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ResizePaneArgs>() };
            copy->_ResizeDirection = _ResizeDirection;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_ResizeDirection);
        }
    };

    struct MoveFocusArgs : public MoveFocusArgsT<MoveFocusArgs>
    {
        MoveFocusArgs() = default;
        MoveFocusArgs(Model::FocusDirection direction) :
            _FocusDirection{ direction } {};

        WINRT_PROPERTY(Model::FocusDirection, FocusDirection, FocusDirection::None);

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
            if (args->_FocusDirection == FocusDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<MoveFocusArgs>() };
            copy->_FocusDirection = _FocusDirection;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_FocusDirection);
        }
    };

    struct AdjustFontSizeArgs : public AdjustFontSizeArgsT<AdjustFontSizeArgs>
    {
        AdjustFontSizeArgs() = default;
        WINRT_PROPERTY(int32_t, Delta, 0);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<AdjustFontSizeArgs>() };
            copy->_Delta = _Delta;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Delta);
        }
    };

    struct SendInputArgs : public SendInputArgsT<SendInputArgs>
    {
        SendInputArgs() = default;
        WINRT_PROPERTY(winrt::hstring, Input, L"");

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
            if (args->_Input.empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SendInputArgs>() };
            copy->_Input = _Input;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Input);
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
        WINRT_PROPERTY(SplitState, SplitStyle, SplitState::Automatic);
        WINRT_PROPERTY(Model::NewTerminalArgs, TerminalArgs, nullptr);
        WINRT_PROPERTY(SplitType, SplitMode, SplitType::Manual);
        WINRT_PROPERTY(double, SplitSize, .5);

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
            if (args->_SplitSize >= 1 || args->_SplitSize <= 0)
            {
                return { nullptr, { SettingsLoadWarnings::InvalidSplitSize } };
            }
            return { *args, {} };
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
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_SplitStyle, _TerminalArgs, _SplitMode, _SplitSize);
        }
    };

    struct OpenSettingsArgs : public OpenSettingsArgsT<OpenSettingsArgs>
    {
        OpenSettingsArgs() = default;
        OpenSettingsArgs(const SettingsTarget& target) :
            _Target{ target } {}
        WINRT_PROPERTY(SettingsTarget, Target, SettingsTarget::SettingsFile);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<OpenSettingsArgs>() };
            copy->_Target = _Target;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Target);
        }
    };

    struct SetColorSchemeArgs : public SetColorSchemeArgsT<SetColorSchemeArgs>
    {
        SetColorSchemeArgs() = default;
        SetColorSchemeArgs(winrt::hstring name) :
            _SchemeName{ name } {};
        WINRT_PROPERTY(winrt::hstring, SchemeName, L"");

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
            if (args->_SchemeName.empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SetColorSchemeArgs>() };
            copy->_SchemeName = _SchemeName;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_SchemeName);
        }
    };

    struct SetTabColorArgs : public SetTabColorArgsT<SetTabColorArgs>
    {
        SetTabColorArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, nullptr);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<SetTabColorArgs>() };
            copy->_TabColor = _TabColor;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_TabColor);
        }
    };

    struct RenameTabArgs : public RenameTabArgsT<RenameTabArgs>
    {
        RenameTabArgs() = default;
        WINRT_PROPERTY(winrt::hstring, Title, L"");

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<RenameTabArgs>() };
            copy->_Title = _Title;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Title);
        }
    };

    struct ExecuteCommandlineArgs : public ExecuteCommandlineArgsT<ExecuteCommandlineArgs>
    {
        ExecuteCommandlineArgs() = default;
        ExecuteCommandlineArgs(winrt::hstring commandline) :
            _Commandline{ commandline } {};
        WINRT_PROPERTY(winrt::hstring, Commandline, L"");

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
            if (args->_Commandline.empty())
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ExecuteCommandlineArgs>() };
            copy->_Commandline = _Commandline;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Commandline);
        }
    };

    struct CloseOtherTabsArgs : public CloseOtherTabsArgsT<CloseOtherTabsArgs>
    {
        CloseOtherTabsArgs() = default;
        CloseOtherTabsArgs(uint32_t& tabIndex) :
            _Index{ tabIndex } {};
        WINRT_PROPERTY(Windows::Foundation::IReference<uint32_t>, Index, nullptr);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CloseOtherTabsArgs>() };
            copy->_Index = _Index;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Index);
        }
    };

    struct CloseTabsAfterArgs : public CloseTabsAfterArgsT<CloseTabsAfterArgs>
    {
        CloseTabsAfterArgs() = default;
        CloseTabsAfterArgs(uint32_t& tabIndex) :
            _Index{ tabIndex } {};
        WINRT_PROPERTY(Windows::Foundation::IReference<uint32_t>, Index, nullptr);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<CloseTabsAfterArgs>() };
            copy->_Index = _Index;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Index);
        }
    };

    struct MoveTabArgs : public MoveTabArgsT<MoveTabArgs>
    {
        MoveTabArgs() = default;
        MoveTabArgs(MoveTabDirection direction) :
            _Direction{ direction } {};
        WINRT_PROPERTY(MoveTabDirection, Direction, MoveTabDirection::None);

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
            if (args->_Direction == MoveTabDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<MoveTabArgs>() };
            copy->_Direction = _Direction;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Direction);
        }
    };

    struct ScrollUpArgs : public ScrollUpArgsT<ScrollUpArgs>
    {
        ScrollUpArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::IReference<uint32_t>, RowsToScroll, nullptr);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ScrollUpArgs>() };
            copy->_RowsToScroll = _RowsToScroll;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_RowsToScroll);
        }
    };

    struct ScrollDownArgs : public ScrollDownArgsT<ScrollDownArgs>
    {
        ScrollDownArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::IReference<uint32_t>, RowsToScroll, nullptr);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ScrollDownArgs>() };
            copy->_RowsToScroll = _RowsToScroll;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_RowsToScroll);
        }
    };

    struct ToggleCommandPaletteArgs : public ToggleCommandPaletteArgsT<ToggleCommandPaletteArgs>
    {
        ToggleCommandPaletteArgs() = default;

        // To preserve backward compatibility the default is Action.
        WINRT_PROPERTY(CommandPaletteLaunchMode, LaunchMode, CommandPaletteLaunchMode::Action);

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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<ToggleCommandPaletteArgs>() };
            copy->_LaunchMode = _LaunchMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_LaunchMode);
        }
    };

    struct FindMatchArgs : public FindMatchArgsT<FindMatchArgs>
    {
        FindMatchArgs() = default;
        FindMatchArgs(FindMatchDirection direction) :
            _Direction{ direction } {};
        WINRT_PROPERTY(FindMatchDirection, Direction, FindMatchDirection::None);

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
            if (args->_Direction == FindMatchDirection::None)
            {
                return { nullptr, { SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<FindMatchArgs>() };
            copy->_Direction = _Direction;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Direction);
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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NewWindowArgs>() };
            copy->_TerminalArgs = _TerminalArgs.Copy();
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_TerminalArgs);
        }
    };

    struct PrevTabArgs : public PrevTabArgsT<PrevTabArgs>
    {
        PrevTabArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, nullptr);
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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<PrevTabArgs>() };
            copy->_SwitcherMode = _SwitcherMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_SwitcherMode);
        }
    };

    struct NextTabArgs : public NextTabArgsT<NextTabArgs>
    {
        NextTabArgs() = default;
        WINRT_PROPERTY(Windows::Foundation::IReference<TabSwitcherMode>, SwitcherMode, nullptr);
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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<NextTabArgs>() };
            copy->_SwitcherMode = _SwitcherMode;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_SwitcherMode);
        }
    };

    struct RenameWindowArgs : public RenameWindowArgsT<RenameWindowArgs>
    {
        RenameWindowArgs() = default;
        WINRT_PROPERTY(winrt::hstring, Name);
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
        IActionArgs Copy() const
        {
            auto copy{ winrt::make_self<RenameWindowArgs>() };
            copy->_Name = _Name;
            return *copy;
        }
        size_t Hash() const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Name);
        }
    };

    struct GlobalSummonArgs : public GlobalSummonArgsT<GlobalSummonArgs>
    {
        GlobalSummonArgs() = default;
        WINRT_PROPERTY(winrt::hstring, Name, L"");
        WINRT_PROPERTY(Model::DesktopBehavior, Desktop, Model::DesktopBehavior::ToCurrent);
        WINRT_PROPERTY(Model::MonitorBehavior, Monitor, Model::MonitorBehavior::ToMouse);
        WINRT_PROPERTY(bool, ToggleVisibility, true);
        WINRT_PROPERTY(uint32_t, DropdownDuration, 0);

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
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(_Name, _Desktop, _Monitor, _DropdownDuration, _ToggleVisibility);
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
