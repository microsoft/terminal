// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ActionArgs.h"

#include "ActionEventArgs.g.cpp"
#include "NewTerminalArgs.g.cpp"
#include "CopyTextArgs.g.cpp"
#include "NewTabArgs.g.cpp"
#include "SwitchToTabArgs.g.cpp"
#include "ResizePaneArgs.g.cpp"
#include "MoveFocusArgs.g.cpp"
#include "AdjustFontSizeArgs.g.cpp"
#include "SplitPaneArgs.g.cpp"
#include "OpenSettingsArgs.g.cpp"

#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    winrt::hstring NewTerminalArgs::GenerateName()
    {
        std::wstringstream ss;

        if (!_Profile.empty())
        {
            ss << fmt::format(L"profile: {}, ", _Profile);
        }
        else if (_ProfileIndex)
        {
            ss << fmt::format(L"profile index: {}, ", _ProfileIndex.Value());
        }

        if (!_Commandline.empty())
        {
            ss << fmt::format(L"commandline: {}, ", _Commandline);
        }

        if (!_StartingDirectory.empty())
        {
            ss << fmt::format(L"directory: {}, ", _StartingDirectory);
        }

        if (!_TabTitle.empty())
        {
            ss << fmt::format(L"title: {}, ", _TabTitle);
        }
        auto s = ss.str();
        if (s.empty())
        {
            return L"";
        }

        // Chop off the last ", "
        return winrt::hstring{ s.substr(0, s.size() - 2) };
    }

    winrt::hstring CopyTextArgs::GenerateName()
    {
        if (_SingleLine)
        {
            return RS_(L"CopyTextAsSingleLineCommandKey");
        }
        return RS_(L"CopyTextCommandKey");
    }

    winrt::hstring NewTabArgs::GenerateName()
    {
        winrt::hstring newTerminalArgsStr;
        if (_TerminalArgs)
        {
            newTerminalArgsStr = _TerminalArgs.GenerateName();
        }
        if (newTerminalArgsStr.empty())
        {
            return RS_(L"NewTabCommandKey");
        }
        else
        {
            return winrt::hstring{ fmt::format(L"{}, {}", RS_(L"NewTabCommandKey"), newTerminalArgsStr) };
        }
        return L"NewTabArgs";
    }

    winrt::hstring SwitchToTabArgs::GenerateName()
    {
        return winrt::hstring{ fmt::format(L"{}, index:{}", RS_(L"SwitchToTabCommandKey"), _TabIndex) };
    }

    winrt::hstring ResizePaneArgs::GenerateName()
    {
        winrt::hstring directionString;
        switch (_Direction)
        {
        case Direction::Left:
            directionString = RS_(L"DirectionLeft");
            break;
        case Direction::Right:
            directionString = RS_(L"DirectionRight");
            break;
        case Direction::Up:
            directionString = RS_(L"DirectionUp");
            break;
        case Direction::Down:
            directionString = RS_(L"DirectionDown");
            break;
        }
        return winrt::hstring{ fmt::format(RS_(L"ResizePaneWithArgCommandKey").c_str(), directionString) };
    }

    winrt::hstring MoveFocusArgs::GenerateName()
    {
        winrt::hstring directionString;
        switch (_Direction)
        {
        case Direction::Left:
            directionString = RS_(L"DirectionLeft");
            break;
        case Direction::Right:
            directionString = RS_(L"DirectionRight");
            break;
        case Direction::Up:
            directionString = RS_(L"DirectionUp");
            break;
        case Direction::Down:
            directionString = RS_(L"DirectionDown");
            break;
        }
        return winrt::hstring{ fmt::format(RS_(L"MoveFocusWithArgCommandKey").c_str(), directionString) };
    }

    winrt::hstring AdjustFontSizeArgs::GenerateName()
    {
        if (_Delta < 0)
        {
            return _Delta == -1 ? winrt::hstring{ fmt::format(RS_(L"DecreaseFontSizeCommandKey").c_str()) } :
                                  winrt::hstring{ fmt::format(RS_(L"DecreaseFontSizeWithAmountCommandKey").c_str(), -_Delta) };
        }
        else
        {
            return _Delta == 1 ? winrt::hstring{ fmt::format(RS_(L"IncreaseFontSizeCommandKey").c_str()) } :
                                 winrt::hstring{ fmt::format(RS_(L"IncreaseFontSizeWithAmountCommandKey").c_str(), _Delta) };
        }
    }

    winrt::hstring SplitPaneArgs::GenerateName()
    {
        std::wstringstream ss;
        if (_SplitMode == SplitType::Duplicate)
        {
            ss << RS_(L"DuplicatePaneCommandKey").c_str();
        }
        else
        {
            ss << RS_(L"SplitPaneCommandKey").c_str();
        }
        ss << L", ";

        switch (_SplitStyle)
        {
        case SplitState::Vertical:
            ss << L"direction: Vertical, ";
            break;
        case SplitState::Horizontal:
            ss << L"direction: Horizontal, ";
            break;
        }

        winrt::hstring newTerminalArgsStr;
        if (_TerminalArgs)
        {
            newTerminalArgsStr = _TerminalArgs.GenerateName();
        }

        if (_SplitMode != SplitType::Duplicate && !newTerminalArgsStr.empty())
        {
            ss << newTerminalArgsStr.c_str();
            ss << L", ";
        }

        // Chop off the last ", "
        auto s = ss.str();
        return winrt::hstring{ s.substr(0, s.size() - 2) };
    }

    winrt::hstring OpenSettingsArgs::GenerateName()
    {
        switch (_Target)
        {
        case SettingsTarget::DefaultsFile:
            return RS_(L"OpenDefaultSettingsCommandKey");
        case SettingsTarget::AllFiles:
            return RS_(L"OpenBothSettingsFilesCommandKey");
        default:
            return RS_(L"OpenSettingsCommandKey");
        }
    }
}
