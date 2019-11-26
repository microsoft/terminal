// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// HEY YOU: When adding ActionArgs types, make sure to add the corresponding
//          *.g.cpp to ActionArgs.cpp!
#include "ActionEventArgs.g.h"
#include "CopyTextArgs.g.h"
#include "NewTabArgs.g.h"
#include "SwitchToTabArgs.g.h"
#include "ResizePaneArgs.g.h"
#include "MoveFocusArgs.g.h"
#include "AdjustFontSizeArgs.g.h"

#include "../../cascadia/inc/cppwinrt_utils.h"
#include "Utils.h"

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as an ActionArgs
//   class that implements IActionArgs
// * ActionEventArgs holds a single IActionArgs. For events that don't need
//   additional args, this can be nullptr.

namespace winrt::TerminalApp::implementation
{
    struct ActionEventArgs : public ActionEventArgsT<ActionEventArgs>
    {
        ActionEventArgs() = default;
        ActionEventArgs(const TerminalApp::IActionArgs& args) :
            _ActionArgs{ args } {};
        GETSET_PROPERTY(IActionArgs, ActionArgs, nullptr);
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        GETSET_PROPERTY(bool, TrimWhitespace, false);

        static constexpr std::string_view TrimWhitespaceKey{ "trimWhitespace" };

    public:
        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<CopyTextArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_TrimWhitespace == _TrimWhitespace;
            }
            return false;
        };
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<CopyTextArgs>();
            if (auto trimWhitespace{ json[JsonKey(TrimWhitespaceKey)] })
            {
                args->_TrimWhitespace = trimWhitespace.asBool();
            }
            return *args;
        }
    };

    struct NewTabArgs : public NewTabArgsT<NewTabArgs>
    {
        NewTabArgs() = default;
        GETSET_PROPERTY(Windows::Foundation::IReference<int32_t>, ProfileIndex, nullptr);

        static constexpr std::string_view ProfileIndexKey{ "index" };

    public:
        bool Equals(const IActionArgs& other)
        {
            auto otherAsUs = other.try_as<NewTabArgs>();
            if (otherAsUs)
            {
                return otherAsUs->_ProfileIndex == _ProfileIndex;
            }
            return false;
        };
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<NewTabArgs>();
            if (auto profileIndex{ json[JsonKey(ProfileIndexKey)] })
            {
                args->_ProfileIndex = profileIndex.asInt();
            }
            return *args;
        }
    };

    struct SwitchToTabArgs : public SwitchToTabArgsT<SwitchToTabArgs>
    {
        SwitchToTabArgs() = default;
        GETSET_PROPERTY(int32_t, TabIndex, 0);

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
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<SwitchToTabArgs>();
            if (auto tabIndex{ json[JsonKey(TabIndexKey)] })
            {
                args->_TabIndex = tabIndex.asInt();
            }
            return *args;
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
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<ResizePaneArgs>();
            if (auto directionString{ json[JsonKey(DirectionKey)] })
            {
                args->_Direction = ParseDirection(directionString.asString());
            }
            return *args;
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
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<MoveFocusArgs>();
            if (auto directionString{ json[JsonKey(DirectionKey)] })
            {
                args->_Direction = ParseDirection(directionString.asString());
            }
            return *args;
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
        static winrt::TerminalApp::IActionArgs FromJson(const Json::Value& json)
        {
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<AdjustFontSizeArgs>();
            if (auto jsonDelta{ json[JsonKey(AdjustFontSizeDelta)] })
            {
                args->_Delta = jsonDelta.asInt();
            }
            return *args;
        }
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionEventArgs);
}
