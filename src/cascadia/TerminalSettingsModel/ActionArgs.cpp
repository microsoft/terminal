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
#include "SendInputArgs.g.cpp"
#include "SplitPaneArgs.g.cpp"
#include "OpenSettingsArgs.g.cpp"
#include "SetColorSchemeArgs.g.cpp"
#include "SetTabColorArgs.g.cpp"
#include "RenameTabArgs.g.cpp"
#include "ExecuteCommandlineArgs.g.cpp"

#include "..\TerminalApp\Utils.h"

#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    winrt::hstring NewTerminalArgs::GenerateName() const
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

    winrt::hstring CopyTextArgs::GenerateName() const
    {
        std::wstringstream ss;

        if (_SingleLine)
        {
            ss << RS_(L"CopyTextAsSingleLineCommandKey").c_str();
        }
        else
        {
            ss << RS_(L"CopyTextCommandKey").c_str();
        }

        if (_CopyFormatting != nullptr)
        {
            ss << L", copyFormatting: ";
            if (_CopyFormatting.Value() == CopyFormat::All)
            {
                ss << L"all, ";
            }
            else if (_CopyFormatting.Value() == static_cast<CopyFormat>(0))
            {
                ss << L"none, ";
            }
            else
            {
                if (WI_IsFlagSet(_CopyFormatting.Value(), CopyFormat::HTML))
                {
                    ss << L"html, ";
                }

                if (WI_IsFlagSet(_CopyFormatting.Value(), CopyFormat::RTF))
                {
                    ss << L"rtf, ";
                }
            }

            // Chop off the last ","
            auto result = ss.str();
            return winrt::hstring{ result.substr(0, result.size() - 2) };
        }

        return winrt::hstring{ ss.str() };
    }

    winrt::hstring NewTabArgs::GenerateName() const
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
        return winrt::hstring{
            fmt::format(L"{}, {}", RS_(L"NewTabCommandKey"), newTerminalArgsStr)
        };
    }

    winrt::hstring SwitchToTabArgs::GenerateName() const
    {
        return winrt::hstring{
            fmt::format(L"{}, index:{}", RS_(L"SwitchToTabCommandKey"), _TabIndex)
        };
    }

    winrt::hstring ResizePaneArgs::GenerateName() const
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
        return winrt::hstring{
            fmt::format(std::wstring_view(RS_(L"ResizePaneWithArgCommandKey")),
                        directionString)
        };
    }

    winrt::hstring MoveFocusArgs::GenerateName() const
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
        return winrt::hstring{
            fmt::format(std::wstring_view(RS_(L"MoveFocusWithArgCommandKey")),
                        directionString)
        };
    }

    winrt::hstring AdjustFontSizeArgs::GenerateName() const
    {
        // If the amount is just 1 (or -1), we'll just return "Increase font
        // size" (or "Decrease font size"). If the amount delta has a greater
        // absolute value, we'll include it like"
        // * Decrease font size, amount: {delta}"
        if (_Delta < 0)
        {
            return _Delta == -1 ? RS_(L"DecreaseFontSizeCommandKey") :
                                  winrt::hstring{
                                      fmt::format(std::wstring_view(RS_(L"DecreaseFontSizeWithAmountCommandKey")),
                                                  -_Delta)
                                  };
        }
        else
        {
            return _Delta == 1 ? RS_(L"IncreaseFontSizeCommandKey") :
                                 winrt::hstring{
                                     fmt::format(std::wstring_view(RS_(L"IncreaseFontSizeWithAmountCommandKey")),
                                                 _Delta)
                                 };
        }
    }

    winrt::hstring SendInputArgs::GenerateName() const
    {
        // The string will be similar to the following:
        // * "Send Input: ...input..."

        auto escapedInput = VisualizeControlCodes(_Input);
        auto name = fmt::format(std::wstring_view(RS_(L"SendInputCommandKey")), escapedInput);
        return winrt::hstring{ name };
    }

    winrt::hstring SplitPaneArgs::GenerateName() const
    {
        // The string will be similar to the following:
        // * "Duplicate pane[, split: <direction>][, new terminal arguments...]"
        // * "Split pane[, split: <direction>][, new terminal arguments...]"
        //
        // Direction will only be added to the string if the split direction is
        // not "auto".
        // If this is a "duplicate pane" action, then the new terminal arguments
        // will be omitted (as they're unused)

        std::wstringstream ss;
        if (_SplitMode == SplitType::Duplicate)
        {
            ss << std::wstring_view(RS_(L"DuplicatePaneCommandKey"));
        }
        else
        {
            ss << std::wstring_view(RS_(L"SplitPaneCommandKey"));
        }
        ss << L", ";

        // This text is intentionally _not_ localized, to attempt to mirror the
        // exact syntax that the property would have in JSON.
        switch (_SplitStyle)
        {
        case SplitState::Vertical:
            ss << L"split: vertical, ";
            break;
        case SplitState::Horizontal:
            ss << L"split: horizontal, ";
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

    winrt::hstring OpenSettingsArgs::GenerateName() const
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

    winrt::hstring SetColorSchemeArgs::GenerateName() const
    {
        // "Set color scheme to "{_SchemeName}""
        if (!_SchemeName.empty())
        {
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"SetColorSchemeCommandKey")),
                            _SchemeName.c_str())
            };
        }
        return L"";
    }

    winrt::hstring SetTabColorArgs::GenerateName() const
    {
        // "Set tab color to #RRGGBB"
        // "Reset tab color"
        if (_TabColor)
        {
            til::color c{ _TabColor.Value() };
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"SetTabColorCommandKey")),
                            c.ToHexString(true))
            };
        }

        return RS_(L"ResetTabColorCommandKey");
    }

    winrt::hstring RenameTabArgs::GenerateName() const
    {
        // "Rename tab to \"{_Title}\""
        // "Reset tab title"
        if (!_Title.empty())
        {
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"RenameTabCommandKey")),
                            _Title.c_str())
            };
        }
        return RS_(L"ResetTabNameCommandKey");
    }

    winrt::hstring ExecuteCommandlineArgs::GenerateName() const
    {
        // "Run commandline "{_Commandline}" in this window"
        if (!_Commandline.empty())
        {
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"ExecuteCommandlineCommandKey")),
                            _Commandline.c_str())
            };
        }
        return L"";
    }

    winrt::hstring CloseOtherTabsArgs::GenerateName() const
    {
        if (_Index)
        {
            // "Close tabs other than index {0}"
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"CloseOtherTabsCommandKey")),
                            _Index.Value())
            };
        }
        return RS_(L"CloseOtherTabsDefaultCommandKey");
    }

    winrt::hstring CloseTabsAfterArgs::GenerateName() const
    {
        if (_Index)
        {
            // "Close tabs after index {0}"
            return winrt::hstring{
                fmt::format(std::wstring_view(RS_(L"CloseTabsAfterCommandKey")),
                            _Index.Value())
            };
        }
        return RS_(L"CloseTabsAfterDefaultCommandKey");
    }
}
