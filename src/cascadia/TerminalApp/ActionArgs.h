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
#include "MoveSelectionPointArgs.g.h"
#include "SplitPaneArgs.g.h"

#include <winrt/Microsoft.Terminal.Settings.h>
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

        static constexpr std::string_view SingleLineKey{ "singleLine" };

    public:
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
            if (auto singleLine{ json[JsonKey(SingleLineKey)] })
            {
                args->_SingleLine = singleLine.asBool();
            }
            return { *args, {} };
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
    static winrt::Microsoft::Terminal::Settings::Direction ParseDirection(const std::string& directionString)
    {
        if (directionString == LeftString)
        {
            return winrt::Microsoft::Terminal::Settings::Direction::Left;
        }
        else if (directionString == RightString)
        {
            return winrt::Microsoft::Terminal::Settings::Direction::Right;
        }
        else if (directionString == UpString)
        {
            return winrt::Microsoft::Terminal::Settings::Direction::Up;
        }
        else if (directionString == DownString)
        {
            return winrt::Microsoft::Terminal::Settings::Direction::Down;
        }
        // default behavior for invalid data
        return winrt::Microsoft::Terminal::Settings::Direction::None;
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Direction, Direction, winrt::Microsoft::Terminal::Settings::Direction::None);

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
            if (args->_Direction == winrt::Microsoft::Terminal::Settings::Direction::None)
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
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Direction, Direction, winrt::Microsoft::Terminal::Settings::Direction::None);

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
            if (args->_Direction == winrt::Microsoft::Terminal::Settings::Direction::None)
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

    // Possible SelectionExpansionMode values
    // TODO:GH#2550/#3475 - move these to a centralized deserializing place
    static constexpr std::string_view CellString{ "cell" };
    static constexpr std::string_view WordString{ "word" };
    static constexpr std::string_view ViewportString{ "viewport" };
    static constexpr std::string_view BufferString{ "buffer" };

    // Function Description:
    // - Helper function for parsing a SelectionExpansionMode from a string
    // Arguments:
    // - directionString: the string to attempt to parse
    // Return Value:
    // - The encoded Direction value, or Direction::None if it was an invalid string
    static winrt::Microsoft::Terminal::Settings::SelectionExpansionMode ParseExpansionMode(const std::string& expansionModeString)
    {
        if (expansionModeString == CellString)
        {
            return winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Cell;
        }
        else if (expansionModeString == WordString)
        {
            return winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Word;
        }
        else if (expansionModeString == ViewportString)
        {
            return winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Viewport;
        }
        else if (expansionModeString == BufferString)
        {
            return winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Buffer;
        }
        // default behavior for invalid data
        return winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Cell;
    };

    struct MoveSelectionPointArgs : public MoveSelectionPointArgsT<MoveSelectionPointArgs>
    {
        MoveSelectionPointArgs() = default;
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Direction, Direction, winrt::Microsoft::Terminal::Settings::Direction::None);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::SelectionExpansionMode, ExpansionMode, winrt::Microsoft::Terminal::Settings::SelectionExpansionMode::Cell);

        static constexpr std::string_view DirectionKey{ "direction" };
        static constexpr std::string_view ExpansionModeKey{ "expansionMode" };

    public:
        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<MoveSelectionPointArgs>();
            if (otherAsUs)
            {
                return (otherAsUs->_Direction == _Direction) && (otherAsUs->_ExpansionMode == _ExpansionMode);
            }
            return false;
        };
        static FromJsonResult FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MoveSelectionPointArgs>();
            if (auto directionString{ json[JsonKey(DirectionKey)] })
            {
                args->_Direction = ParseDirection(directionString.asString());

                if (auto expansionModeString{ json[JsonKey(ExpansionModeKey)] })
                {
                    args->_ExpansionMode = ParseExpansionMode(expansionModeString.asString());
                }
            }
            if (args->_Direction == winrt::Microsoft::Terminal::Settings::Direction::None)
            {
                return { nullptr, { ::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter } };
            }
            else
            {
                return { *args, {} };
            }
        }
    };

    // Possible SplitState values
    // TODO:GH#2550/#3475 - move these to a centralized deserializing place
    static constexpr std::string_view VerticalKey{ "vertical" };
    static constexpr std::string_view HorizontalKey{ "horizontal" };
    static constexpr std::string_view AutomaticKey{ "auto" };
    static winrt::Microsoft::Terminal::Settings::SplitState ParseSplitState(const std::string& stateString)
    {
        if (stateString == VerticalKey)
        {
            return winrt::Microsoft::Terminal::Settings::SplitState::Vertical;
        }
        else if (stateString == HorizontalKey)
        {
            return winrt::Microsoft::Terminal::Settings::SplitState::Horizontal;
        }
        else if (stateString == AutomaticKey)
        {
            return winrt::Microsoft::Terminal::Settings::SplitState::Automatic;
        }
        // default behavior for invalid data
        return winrt::Microsoft::Terminal::Settings::SplitState::Automatic;
    };

    // Possible SplitType values
    static constexpr std::string_view DuplicateKey{ "duplicate" };
    static winrt::Microsoft::Terminal::Settings::SplitType ParseSplitModeState(const std::string& stateString)
    {
        if (stateString == DuplicateKey)
        {
            return winrt::Microsoft::Terminal::Settings::SplitType::Duplicate;
        }
        return winrt::Microsoft::Terminal::Settings::SplitType::Manual;
    }

    struct SplitPaneArgs : public SplitPaneArgsT<SplitPaneArgs>
    {
        SplitPaneArgs() = default;
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::SplitState, SplitStyle, winrt::Microsoft::Terminal::Settings::SplitState::Automatic);
        GETSET_PROPERTY(winrt::TerminalApp::NewTerminalArgs, TerminalArgs, nullptr);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::SplitType, SplitMode, winrt::Microsoft::Terminal::Settings::SplitType::Manual);

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
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
    BASIC_FACTORY(NewTerminalArgs);
}
