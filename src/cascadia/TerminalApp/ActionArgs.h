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

#include "../../cascadia/inc/cppwinrt_utils.h"
#include "Utils.h"
#include "TerminalWarnings.h"

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as an ActionArgs
//   class that implements IActionArgs
// * ActionEventArgs holds a single IActionArgs. For events that don't need
//   additional args, this can be nullptr.

namespace winrt::TerminalApp::implementation
{
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
            if (auto commandline{ json[JsonKey(CommandlineKey)] })
            {
                args->_Commandline = winrt::to_hstring(commandline.asString());
            }
            if (auto startingDirectory{ json[JsonKey(StartingDirectoryKey)] })
            {
                args->_StartingDirectory = winrt::to_hstring(startingDirectory.asString());
            }
            if (auto tabTitle{ json[JsonKey(TabTitleKey)] })
            {
                args->_TabTitle = winrt::to_hstring(tabTitle.asString());
            }
            if (auto index{ json[JsonKey(ProfileIndexKey)] })
            {
                args->_ProfileIndex = index.asInt();
            }
            if (auto profile{ json[JsonKey(ProfileKey)] })
            {
                args->_Profile = winrt::to_hstring(profile.asString());
            }
            return *args;
        }
    };

    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        GETSET_PROPERTY(bool, SingleLine, false);
        GETSET_PROPERTY(int, CopyFormatting, -1);

        static constexpr std::string_view SingleLineKey{ "singleLine" };
        static constexpr std::string_view CopyFormattingKey{ "copyFormatting" };

    public:
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
            if (auto singleLine{ json[JsonKey(SingleLineKey)] })
            {
                args->_SingleLine = singleLine.asBool();
            }
            if (auto copyFormatting{ json[JsonKey(CopyFormattingKey)] })
            {
                args->_CopyFormatting = _ParseCopyFormatting(copyFormatting);
            }
            return { *args, {} };
        }

        // accepted copyFormatting values
        static constexpr std::string_view PlainKey{ "plain" };
        static constexpr std::string_view HtmlKey{ "html" };
        static constexpr std::string_view RtfKey{ "rtf" };

        // Method Description:
        // - Helper function for converting the user-specified copyFormatting value(s)
        //   to a set of CopyFormat enum values
        // Arguments:
        // - json: The string value from the settings file to parse
        // Return Value:
        // - The serialized enum values which map to the bool or array of strings provided by the user
        // - -1 represents parsing failure or null. Behavior fallsback to the global setting.
        static int _ParseCopyFormatting(const Json::Value& json) noexcept
        {
            if (json.isArray())
            {
                int result = 0;
                for (const auto value : json)
                {
                    const auto format = value.asString();
                    if (format == PlainKey)
                    {
                        result |= static_cast<int>(CopyFormat::Plain);
                    }
                    else if (format == HtmlKey)
                    {
                        result |= static_cast<int>(CopyFormat::HTML);
                    }
                    if (format == RtfKey)
                    {
                        result |= static_cast<int>(CopyFormat::RTF);
                    }
                }
                return result;
            }
            else if (json.isBool())
            {
                if (json.asBool())
                {
                    return static_cast<int>(CopyFormat::Plain) |
                           static_cast<int>(CopyFormat::HTML) |
                           static_cast<int>(CopyFormat::RTF);
                }
                else
                {
                    return static_cast<int>(CopyFormat::Plain);
                }
            }
            return -1;
        }
    };

    struct NewTabArgs : public NewTabArgsT<NewTabArgs>
    {
        NewTabArgs() = default;
        GETSET_PROPERTY(winrt::TerminalApp::NewTerminalArgs, TerminalArgs, nullptr);

    public:
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
            if (auto tabIndex{ json[JsonKey(TabIndexKey)] })
            {
                args->_TabIndex = tabIndex.asUInt();
            }
            return { *args, {} };
        }
    };

    // Possible Direction values
    // TODO:GH#2550/#3475 - move these to a centralized deserializing place
    static constexpr std::string_view LeftString{ "left" };
    static constexpr std::string_view RightString{ "right" };
    static constexpr std::string_view UpString{ "up" };
    static constexpr std::string_view DownString{ "down" };

    // Function Description:
    // - Helper function for parsing a Direction from a string
    // Arguments:
    // - directionString: the string to attempt to parse
    // Return Value:
    // - The encoded Direction value, or Direction::None if it was an invalid string
    static TerminalApp::Direction ParseDirection(const std::string& directionString)
    {
        if (directionString == LeftString)
        {
            return TerminalApp::Direction::Left;
        }
        else if (directionString == RightString)
        {
            return TerminalApp::Direction::Right;
        }
        else if (directionString == UpString)
        {
            return TerminalApp::Direction::Up;
        }
        else if (directionString == DownString)
        {
            return TerminalApp::Direction::Down;
        }
        // default behavior for invalid data
        return TerminalApp::Direction::None;
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::None);

        static constexpr std::string_view DirectionKey{ "direction" };

    public:
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
            if (auto directionString{ json[JsonKey(DirectionKey)] })
            {
                args->_Direction = ParseDirection(directionString.asString());
            }
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
            if (auto directionString{ json[JsonKey(DirectionKey)] })
            {
                args->_Direction = ParseDirection(directionString.asString());
            }
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
            if (auto jsonDelta{ json[JsonKey(AdjustFontSizeDelta)] })
            {
                args->_Delta = jsonDelta.asInt();
            }
            return { *args, {} };
        }
    };

    // Possible SplitState values
    // TODO:GH#2550/#3475 - move these to a centralized deserializing place
    static constexpr std::string_view VerticalKey{ "vertical" };
    static constexpr std::string_view HorizontalKey{ "horizontal" };
    static constexpr std::string_view AutomaticKey{ "auto" };
    static TerminalApp::SplitState ParseSplitState(const std::string& stateString)
    {
        if (stateString == VerticalKey)
        {
            return TerminalApp::SplitState::Vertical;
        }
        else if (stateString == HorizontalKey)
        {
            return TerminalApp::SplitState::Horizontal;
        }
        else if (stateString == AutomaticKey)
        {
            return TerminalApp::SplitState::Automatic;
        }
        // default behavior for invalid data
        return TerminalApp::SplitState::Automatic;
    };

    // Possible SplitType values
    static constexpr std::string_view DuplicateKey{ "duplicate" };
    static TerminalApp::SplitType ParseSplitModeState(const std::string& stateString)
    {
        if (stateString == DuplicateKey)
        {
            return TerminalApp::SplitType::Duplicate;
        }
        return TerminalApp::SplitType::Manual;
    }

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        GETSET_PROPERTY(winrt::TerminalApp::SplitState, SplitStyle, winrt::TerminalApp::SplitState::Automatic);
        GETSET_PROPERTY(winrt::TerminalApp::NewTerminalArgs, TerminalArgs, nullptr);
        GETSET_PROPERTY(winrt::TerminalApp::SplitType, SplitMode, winrt::TerminalApp::SplitType::Manual);

        static constexpr std::string_view SplitKey{ "split" };
        static constexpr std::string_view SplitModeKey{ "splitMode" };

    public:
        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<SplitPaneArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_SplitStyle == _SplitStyle &&
                       otherAsUs->_TerminalArgs == _TerminalArgs &&
                       otherAsUs->_SplitMode == _SplitMode;
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SplitPaneArgs>();
            args->_TerminalArgs = NewTerminalArgs::FromJson(json);
            if (auto jsonStyle{ json[JsonKey(SplitKey)] })
            {
                args->_SplitStyle = ParseSplitState(jsonStyle.asString());
            }
            if (auto jsonStyle{ json[JsonKey(SplitModeKey)] })
            {
                args->_SplitMode = ParseSplitModeState(jsonStyle.asString());
            }
            return { *args, {} };
        }
    };

    // Possible SettingsTarget values
    // TODO:GH#2550/#3475 - move these to a centralized deserializing place
    static constexpr std::string_view SettingsFileString{ "settingsFile" };
    static constexpr std::string_view DefaultsFileString{ "defaultsFile" };
    static constexpr std::string_view AllFilesString{ "allFiles" };

    // Function Description:
    // - Helper function for parsing a SettingsTarget from a string
    // Arguments:
    // - targetString: the string to attempt to parse
    // Return Value:
    // - The encoded SettingsTarget value, or SettingsTarget::SettingsFile if it was an invalid string
    static TerminalApp::SettingsTarget ParseSettingsTarget(const std::string& targetString)
    {
        if (targetString == SettingsFileString)
        {
            return TerminalApp::SettingsTarget::SettingsFile;
        }
        else if (targetString == DefaultsFileString)
        {
            return TerminalApp::SettingsTarget::DefaultsFile;
        }
        else if (targetString == AllFilesString)
        {
            return TerminalApp::SettingsTarget::AllFiles;
        }
        // default behavior for invalid data
        return TerminalApp::SettingsTarget::SettingsFile;
    };

    struct OpenSettingsArgs : public OpenSettingsArgsT<OpenSettingsArgs>
    {
        OpenSettingsArgs() = default;
        GETSET_PROPERTY(TerminalApp::SettingsTarget, Target, TerminalApp::SettingsTarget::SettingsFile);

        static constexpr std::string_view TargetKey{ "target" };

    public:
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
            if (auto targetString{ json[JsonKey(TargetKey)] })
            {
                args->_Target = ParseSettingsTarget(targetString.asString());
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
