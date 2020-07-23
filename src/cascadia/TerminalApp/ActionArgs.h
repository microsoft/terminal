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
#include "SplitPaneArgs.g.h"
#include "OpenSettingsArgs.g.h"
#include "SetTabColorArgs.g.h"
#include "RenameTabArgs.g.h"
#include "ExecuteCommandlineArgs.g.h"

#include "../../cascadia/inc/cppwinrt_utils.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalWarnings.h"

#include "TerminalSettingsSerializationHelpers.h"

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as an ActionArgs
//   class that implements IActionArgs
// * ActionEventArgs holds a single IActionArgs. For events that don't need
//   additional args, this can be nullptr.

namespace winrt::TerminalApp::implementation
{
    using namespace ::TerminalApp;
    using FromJsonResult = std::tuple<winrt::TerminalApp::IActionArgs, std::vector<::TerminalApp::SettingsLoadWarnings>>;

    struct ActionEventArgs : public ActionEventArgsT<ActionEventArgs>
    {
        ActionEventArgs() = default;

        explicit ActionEventArgs(const TerminalApp::IActionArgs& args) :
            _ActionArgs{ args } {};
        GETSET_PROPERTY(IActionArgs, ActionArgs, nullptr);
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct NewTerminalArgs : public NewTerminalArgsT<NewTerminalArgs>
    {
        NewTerminalArgs() = default;
        GETSET_PROPERTY(winrt::hstring, Commandline, L"");
        GETSET_PROPERTY(winrt::hstring, StartingDirectory, L"");
        GETSET_PROPERTY(winrt::hstring, TabTitle, L"");
        GETSET_PROPERTY(Windows::Foundation::IReference<int32_t>, ProfileIndex, nullptr);
        GETSET_PROPERTY(winrt::hstring, Profile, L"");

        static constexpr std::string_view CommandlineKey{ "commandline" };
        static constexpr std::string_view StartingDirectoryKey{ "startingDirectory" };
        static constexpr std::string_view TabTitleKey{ "tabTitle" };
        static constexpr std::string_view ProfileIndexKey{ "index" };
        static constexpr std::string_view ProfileKey{ "profile" };

    public:
        hstring GenerateName() const;

        bool Equals(const winrt::TerminalApp::NewTerminalArgs& other)
        {
            return other.Commandline() == _Commandline &&
                   other.StartingDirectory() == _StartingDirectory &&
                   other.TabTitle() == _TabTitle &&
                   other.ProfileIndex() == _ProfileIndex &&
                   other.Profile() == _Profile;
        };
        static winrt::TerminalApp::NewTerminalArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewTerminalArgs>();
            JsonUtils::GetValueForKey(json, CommandlineKey, args->_Commandline);
            JsonUtils::GetValueForKey(json, StartingDirectoryKey, args->_StartingDirectory);
            JsonUtils::GetValueForKey(json, TabTitleKey, args->_TabTitle);
            JsonUtils::GetValueForKey(json, ProfileIndexKey, args->_ProfileIndex);
            JsonUtils::GetValueForKey(json, ProfileKey, args->_Profile);
            return *args;
        }
    };

    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        GETSET_PROPERTY(bool, SingleLine, false);

        static constexpr std::string_view SingleLineKey{ "singleLine" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<CopyTextArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SingleLine == _SingleLine;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<CopyTextArgs>();
            JsonUtils::GetValueForKey(json, SingleLineKey, args->_SingleLine);
            return { *args, {} };
        }
    };

    struct NewTabArgs : public NewTabArgsT<NewTabArgs>
    {
        NewTabArgs() = default;
        GETSET_PROPERTY(winrt::TerminalApp::NewTerminalArgs, TerminalArgs, nullptr);

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
    };

    struct SwitchToTabArgs : public SwitchToTabArgsT<SwitchToTabArgs>
    {
        SwitchToTabArgs() = default;
        GETSET_PROPERTY(uint32_t, TabIndex, 0);

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
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<ResizePaneArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Direction == _Direction;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ResizePaneArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_Direction);
            if (args->_Direction == TerminalApp::Direction::None)
            {
                return { nullptr, { ::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
    };

    struct MoveFocusArgs : public MoveFocusArgsT<MoveFocusArgs>
    {
        MoveFocusArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MoveFocusArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_Direction == _Direction;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MoveFocusArgs>();
            JsonUtils::GetValueForKey(json, DirectionKey, args->_Direction);
            if (args->_Direction == TerminalApp::Direction::None)
            {
                return { nullptr, { ::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
    };

    struct AdjustFontSizeArgs : public AdjustFontSizeArgsT<AdjustFontSizeArgs>
    {
        AdjustFontSizeArgs() = default;
        GETSET_PROPERTY(int32_t, Delta, 0);

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
    };

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        GETSET_PROPERTY(winrt::TerminalApp::SplitState, SplitStyle, winrt::TerminalApp::SplitState::Automatic);
        GETSET_PROPERTY(winrt::TerminalApp::NewTerminalArgs, TerminalArgs, nullptr);
        GETSET_PROPERTY(winrt::TerminalApp::SplitType, SplitMode, winrt::TerminalApp::SplitType::Manual);

        static constexpr std::string_view SplitKey{ "split" };
        static constexpr std::string_view SplitModeKey{ "splitMode" };

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
            return { *args, {} };
        }
    };

    struct OpenSettingsArgs : public OpenSettingsArgsT<OpenSettingsArgs>
    {
        OpenSettingsArgs() = default;
        GETSET_PROPERTY(TerminalApp::SettingsTarget, Target, TerminalApp::SettingsTarget::SettingsFile);

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
    };

    struct SetTabColorArgs : public SetTabColorArgsT<SetTabColorArgs>
    {
        SetTabColorArgs() = default;
        GETSET_PROPERTY(Windows::Foundation::IReference<uint32_t>, TabColor, nullptr);

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
            if (const auto temp{ JsonUtils::GetValueForKey<std::optional<til::color>>(json, ColorKey) })
            {
                args->_TabColor = static_cast<uint32_t>(*temp);
            }
            return { *args, {} };
        }
    };

    struct RenameTabArgs : public RenameTabArgsT<RenameTabArgs>
    {
        RenameTabArgs() = default;
        GETSET_PROPERTY(winrt::hstring, Title, L"");

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
    };

    struct ExecuteCommandlineArgs : public ExecuteCommandlineArgsT<ExecuteCommandlineArgs>
    {
        ExecuteCommandlineArgs() = default;
        GETSET_PROPERTY(winrt::hstring, Commandline, L"");

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
                return { nullptr, { ::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter } };
            }
            return { *args, {} };
        }
    };

}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
    BASIC_FACTORY(NewTerminalArgs);
}
