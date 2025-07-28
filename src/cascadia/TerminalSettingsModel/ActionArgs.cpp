// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ActionArgs.h"

#include "ActionEventArgs.g.cpp"
#include "BaseContentArgs.g.cpp"
#include "NewTerminalArgs.g.cpp"
#include "CopyTextArgs.g.cpp"
#include "NewTabArgs.g.cpp"
#include "SwitchToTabArgs.g.cpp"
#include "ResizePaneArgs.g.cpp"
#include "MoveFocusArgs.g.cpp"
#include "MovePaneArgs.g.cpp"
#include "MoveTabArgs.g.cpp"
#include "SwapPaneArgs.g.cpp"
#include "AdjustFontSizeArgs.g.cpp"
#include "SendInputArgs.g.cpp"
#include "SplitPaneArgs.g.cpp"
#include "OpenSettingsArgs.g.cpp"
#include "SetFocusModeArgs.g.cpp"
#include "SetFullScreenArgs.g.cpp"
#include "SetMaximizedArgs.g.cpp"
#include "SetColorSchemeArgs.g.cpp"
#include "SetTabColorArgs.g.cpp"
#include "RenameTabArgs.g.cpp"
#include "ExecuteCommandlineArgs.g.cpp"
#include "CloseOtherTabsArgs.g.cpp"
#include "CloseTabsAfterArgs.g.cpp"
#include "CloseTabArgs.g.cpp"
#include "ScrollToMarkArgs.g.cpp"
#include "AddMarkArgs.g.cpp"
#include "FindMatchArgs.g.cpp"
#include "SaveSnippetArgs.g.cpp"
#include "ToggleCommandPaletteArgs.g.cpp"
#include "SuggestionsArgs.g.cpp"
#include "NewWindowArgs.g.cpp"
#include "PrevTabArgs.g.cpp"
#include "NextTabArgs.g.cpp"
#include "RenameWindowArgs.g.cpp"
#include "SearchForTextArgs.g.cpp"
#include "GlobalSummonArgs.g.cpp"
#include "FocusPaneArgs.g.cpp"
#include "ExportBufferArgs.g.cpp"
#include "ClearBufferArgs.g.cpp"
#include "MultipleActionsArgs.g.cpp"
#include "AdjustOpacityArgs.g.cpp"
#include "SelectCommandArgs.g.cpp"
#include "SelectOutputArgs.g.cpp"
#include "ColorSelectionArgs.g.cpp"

#include <LibraryResources.h>
#include <WtExeUtils.h>
#include <ScopedResourceLoader.h>

namespace winrt
{
    namespace WARC = ::winrt::Windows::ApplicationModel::Resources::Core;
}

// Like RS_ and RS_fmt, but they use an ambient boolean named "localized" to
// determine whether to load the English version of a resource or the localized
// one.
#define RS_switchable_(x) RS_switchable_impl(context, USES_RESOURCE(x))
#define RS_switchable_fmt(x, ...) RS_switchable_fmt_impl(context, USES_RESOURCE(x), __VA_ARGS__)

static winrt::hstring RS_switchable_impl(const winrt::WARC::ResourceContext& context, std::wstring_view key)
{
    return GetLibraryResourceLoader().ResourceMap().GetValue(key, context).ValueAsString();
}

template<typename... Args>
static std::wstring RS_switchable_fmt_impl(const winrt::WARC::ResourceContext& context, std::wstring_view key, Args&&... args)
{
    const auto format = RS_switchable_impl(context, key);
    return fmt::format(fmt::runtime(std::wstring_view{ format }), std::forward<Args>(args)...);
}

using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    const ScopedResourceLoader& EnglishOnlyResourceLoader() noexcept
    {
        static ScopedResourceLoader loader{ GetLibraryResourceLoader().WithQualifier(L"language", L"en-US") };
        return loader;
    }

    winrt::hstring NewTerminalArgs::GenerateName(const winrt::WARC::ResourceContext&) const
    {
        std::wstring str;

        if (!Profile().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"profile: {}, "), Profile());
        }
        else if (ProfileIndex())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"profile index: {}, "), ProfileIndex().Value());
        }

        if (!Commandline().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"commandline: {}, "), Commandline());
        }

        if (!StartingDirectory().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"directory: {}, "), StartingDirectory());
        }

        if (!TabTitle().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"title: {}, "), TabTitle());
        }

        if (TabColor())
        {
            const til::color tabColor{ TabColor().Value() };
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"tabColor: {}, "), tabColor.ToHexString(true));
        }
        if (!ColorScheme().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"colorScheme: {}, "), ColorScheme());
        }

        if (SuppressApplicationTitle())
        {
            if (SuppressApplicationTitle().Value())
            {
                str.append(L"suppress application title, ");
            }
            else
            {
                str.append(L"use application title, ");
            }
        }

        if (Elevate())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"elevate: {}, "), Elevate().Value());
        }

        if (str.empty())
        {
            return {};
        }

        // Chop off the last ", "
        str.resize(str.size() - 2);
        return winrt::hstring{ str };
    }

    winrt::hstring NewTerminalArgs::ToCommandline() const
    {
        std::wstring str;

        if (!Profile().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--profile \"{}\" "), Profile());
        }

        if (const auto id = SessionId(); id != winrt::guid{})
        {
            const auto idStr = ::Microsoft::Console::Utils::GuidToString(id);
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--sessionId \"{}\" "), idStr);
        }

        // The caller is always expected to provide the evaluated profile in the
        // NewTerminalArgs, not the index
        //
        // else if (ProfileIndex())
        // {
        //     fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"profile index: {}, "), ProfileIndex().Value());
        // }

        if (!StartingDirectory().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--startingDirectory {} "), QuoteAndEscapeCommandlineArg(StartingDirectory()));
        }

        if (!TabTitle().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--title {} "), QuoteAndEscapeCommandlineArg(TabTitle()));
        }

        if (TabColor())
        {
            const til::color tabColor{ TabColor().Value() };
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--tabColor \"{}\" "), tabColor.ToHexString(true));
        }

        if (SuppressApplicationTitle())
        {
            if (SuppressApplicationTitle().Value())
            {
                str.append(L"--suppressApplicationTitle ");
            }
            else
            {
                str.append(L"--useApplicationTitle ");
            }
        }

        if (!ColorScheme().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"--colorScheme {} "), QuoteAndEscapeCommandlineArg(ColorScheme()));
        }

        if (!Commandline().empty())
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"-- \"{}\" "), Commandline());
        }

        if (str.empty())
        {
            return {};
        }

        // Chop off the last " "
        str.resize(str.size() - 1);
        return winrt::hstring{ str };
    }

    winrt::hstring CopyTextArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        std::wstring str;

        if (SingleLine())
        {
            str.append(RS_switchable_(L"CopyTextAsSingleLineCommandKey"));
        }
        else
        {
            str.append(RS_switchable_(L"CopyTextCommandKey"));
        }

        if (WithControlSequences())
        {
            str.append(L", withControlSequences: true");
        }

        if (!DismissSelection())
        {
            str.append(L", dismissSelection: false");
        }

        if (CopyFormatting())
        {
            str.append(L", copyFormatting: ");
            if (CopyFormatting().Value() == CopyFormat::All)
            {
                str.append(L"all, ");
            }
            else if (CopyFormatting().Value() == static_cast<CopyFormat>(0))
            {
                str.append(L"none, ");
            }
            else
            {
                if (WI_IsFlagSet(CopyFormatting().Value(), CopyFormat::HTML))
                {
                    str.append(L"html, ");
                }

                if (WI_IsFlagSet(CopyFormatting().Value(), CopyFormat::RTF))
                {
                    str.append(L"rtf, ");
                }
            }

            // Chop off the last ", "
            str.resize(str.size() - 2);
        }

        return winrt::hstring{ str };
    }

    winrt::hstring NewTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        winrt::hstring newTerminalArgsStr;
        if (ContentArgs())
        {
            newTerminalArgsStr = ContentArgs().GenerateName(context);
        }

        if (newTerminalArgsStr.empty())
        {
            return RS_switchable_(L"NewTabCommandKey");
        }
        return winrt::hstring{
            fmt::format(FMT_COMPILE(L"{}, {}"), RS_switchable_(L"NewTabCommandKey"), newTerminalArgsStr)
        };
    }

    winrt::hstring MovePaneArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (!Window().empty())
        {
            // Special case for moving to a new window. We can just ignore the
            // tab index, because it _doesn't matter_. There won't be any tabs
            // in the new window, till we get there.
            if (Window() == L"new")
            {
                return RS_switchable_(L"MovePaneToNewWindowCommandKey");
            }
            return winrt::hstring{
                fmt::format(FMT_COMPILE(L"{}, window:{}, tab index:{}"), RS_switchable_(L"MovePaneCommandKey"), Window(), TabIndex())
            };
        }
        return winrt::hstring{
            fmt::format(FMT_COMPILE(L"{}, tab index:{}"), RS_switchable_(L"MovePaneCommandKey"), TabIndex())
        };
    }

    winrt::hstring SwitchToTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (TabIndex() == UINT32_MAX)
        {
            return RS_switchable_(L"SwitchToLastTabCommandKey");
        }

        return winrt::hstring{
            fmt::format(FMT_COMPILE(L"{}, index:{}"), RS_switchable_(L"SwitchToTabCommandKey"), TabIndex())
        };
    }

    winrt::hstring ResizePaneArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        winrt::hstring directionString;
        switch (ResizeDirection())
        {
        case ResizeDirection::Left:
            directionString = RS_switchable_(L"DirectionLeft");
            break;
        case ResizeDirection::Right:
            directionString = RS_switchable_(L"DirectionRight");
            break;
        case ResizeDirection::Up:
            directionString = RS_switchable_(L"DirectionUp");
            break;
        case ResizeDirection::Down:
            directionString = RS_switchable_(L"DirectionDown");
            break;
        }
        return winrt::hstring{ RS_switchable_fmt(L"ResizePaneWithArgCommandKey", directionString) };
    }

    winrt::hstring MoveFocusArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        winrt::hstring directionString;
        switch (FocusDirection())
        {
        case FocusDirection::Left:
            directionString = RS_switchable_(L"DirectionLeft");
            break;
        case FocusDirection::Right:
            directionString = RS_switchable_(L"DirectionRight");
            break;
        case FocusDirection::Up:
            directionString = RS_switchable_(L"DirectionUp");
            break;
        case FocusDirection::Down:
            directionString = RS_switchable_(L"DirectionDown");
            break;
        case FocusDirection::Previous:
            return RS_switchable_(L"MoveFocusToLastUsedPane");
        case FocusDirection::NextInOrder:
            return RS_switchable_(L"MoveFocusNextInOrder");
        case FocusDirection::PreviousInOrder:
            return RS_switchable_(L"MoveFocusPreviousInOrder");
        case FocusDirection::First:
            return RS_switchable_(L"MoveFocusFirstPane");
        case FocusDirection::Parent:
            return RS_switchable_(L"MoveFocusParentPane");
        case FocusDirection::Child:
            return RS_switchable_(L"MoveFocusChildPane");
        }

        return winrt::hstring{ RS_switchable_fmt(L"MoveFocusWithArgCommandKey", directionString) };
    }

    winrt::hstring SwapPaneArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        winrt::hstring directionString;
        switch (Direction())
        {
        case FocusDirection::Left:
            directionString = RS_switchable_(L"DirectionLeft");
            break;
        case FocusDirection::Right:
            directionString = RS_switchable_(L"DirectionRight");
            break;
        case FocusDirection::Up:
            directionString = RS_switchable_(L"DirectionUp");
            break;
        case FocusDirection::Down:
            directionString = RS_switchable_(L"DirectionDown");
            break;
        case FocusDirection::Previous:
            return RS_switchable_(L"SwapPaneToLastUsedPane");
        case FocusDirection::NextInOrder:
            return RS_switchable_(L"SwapPaneNextInOrder");
        case FocusDirection::PreviousInOrder:
            return RS_switchable_(L"SwapPanePreviousInOrder");
        case FocusDirection::First:
            return RS_switchable_(L"SwapPaneFirstPane");
        }

        return winrt::hstring{ RS_switchable_fmt(L"SwapPaneWithArgCommandKey", directionString) };
    }

    winrt::hstring AdjustFontSizeArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // If the amount is just 1 (or -1), we'll just return "Increase font
        // size" (or "Decrease font size"). If the amount delta has a greater
        // absolute value, we'll include it like"
        // * Decrease font size, amount: {delta}"
        if (Delta() < 0)
        {
            return Delta() == -1 ? RS_switchable_(L"DecreaseFontSizeCommandKey") : winrt::hstring{ RS_switchable_fmt(L"DecreaseFontSizeWithAmountCommandKey", -Delta()) };
        }
        else
        {
            return Delta() == 1 ? RS_switchable_(L"IncreaseFontSizeCommandKey") : winrt::hstring{ RS_switchable_fmt(L"IncreaseFontSizeWithAmountCommandKey", Delta()) };
        }
    }

    winrt::hstring SendInputArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // The string will be similar to the following:
        // * "Send Input: ...input..."

        const auto escapedInput = til::visualize_control_codes(Input());
        const auto name = RS_switchable_fmt(L"SendInputCommandKey", escapedInput);
        return winrt::hstring{ name };
    }

    winrt::hstring SplitPaneArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // The string will be similar to the following:
        // * "Duplicate pane[, split: <direction>][, size: <size>%][, new terminal arguments...]"
        // * "Split pane[, split: <direction>][, size: <size>%][, new terminal arguments...]"
        //
        // Direction will only be added to the string if the split direction is
        // not "auto".
        // If this is a "duplicate pane" action, then the new terminal arguments
        // will be omitted (as they're unused)

        std::wstring str;
        if (SplitMode() == SplitType::Duplicate)
        {
            str.append(RS_switchable_(L"DuplicatePaneCommandKey"));
        }
        else
        {
            str.append(RS_switchable_(L"SplitPaneCommandKey"));
        }
        str.append(L", ");

        // This text is intentionally _not_ localized, to attempt to mirror the
        // exact syntax that the property would have in JSON.
        switch (SplitDirection())
        {
        case SplitDirection::Up:
            str.append(L"split: up, ");
            break;
        case SplitDirection::Right:
            str.append(L"split: right, ");
            break;
        case SplitDirection::Down:
            str.append(L"split: down, ");
            break;
        case SplitDirection::Left:
            str.append(L"split: left, ");
            break;
        }

        if (SplitSize() != .5f)
        {
            fmt::format_to(std::back_inserter(str), FMT_COMPILE(L"size: {:.2f}%, "), SplitSize() * 100);
        }

        winrt::hstring newTerminalArgsStr;
        if (ContentArgs())
        {
            newTerminalArgsStr = ContentArgs().GenerateName(context);
        }

        if (SplitMode() != SplitType::Duplicate && !newTerminalArgsStr.empty())
        {
            str.append(newTerminalArgsStr);
            str.append(L", ");
        }

        // Chop off the last ", "
        str.resize(str.size() - 2);
        return winrt::hstring{ str };
    }

    winrt::hstring OpenSettingsArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        switch (Target())
        {
        case SettingsTarget::DefaultsFile:
            return RS_switchable_(L"OpenDefaultSettingsCommandKey");
        case SettingsTarget::AllFiles:
            return RS_switchable_(L"OpenBothSettingsFilesCommandKey");
        case SettingsTarget::SettingsFile:
            return RS_switchable_(L"OpenSettingsCommandKey");
        case SettingsTarget::Directory:
            return RS_switchable_(L"SettingsFileOpenInExplorerCommandKey");
        case SettingsTarget::SettingsUI:
        default:
            return RS_switchable_(L"OpenSettingsUICommandKey");
        }
    }

    winrt::hstring SetFocusModeArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (IsFocusMode())
        {
            return RS_switchable_(L"EnableFocusModeCommandKey");
        }
        return RS_switchable_(L"DisableFocusModeCommandKey");
    }

    winrt::hstring SetFullScreenArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (IsFullScreen())
        {
            return RS_switchable_(L"EnableFullScreenCommandKey");
        }
        return RS_switchable_(L"DisableFullScreenCommandKey");
    }

    winrt::hstring SetMaximizedArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (IsMaximized())
        {
            return RS_switchable_(L"EnableMaximizedCommandKey");
        }
        return RS_switchable_(L"DisableMaximizedCommandKey");
    }

    winrt::hstring SetColorSchemeArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Set color scheme to "{_SchemeName}""
        if (!SchemeName().empty())
        {
            return winrt::hstring{ RS_switchable_fmt(L"SetColorSchemeCommandKey", SchemeName()) };
        }
        return {};
    }

    winrt::hstring SetTabColorArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Set tab color to #RRGGBB"
        // "Reset tab color"
        if (TabColor())
        {
            til::color tabColor{ TabColor().Value() };
            return winrt::hstring{ RS_switchable_fmt(L"SetTabColorCommandKey", tabColor.ToHexString(true)) };
        }

        return RS_switchable_(L"ResetTabColorCommandKey");
    }

    winrt::hstring RenameTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Rename tab to \"{_Title}\""
        // "Reset tab title"
        if (!Title().empty())
        {
            return winrt::hstring{ RS_switchable_fmt(L"RenameTabCommandKey", Title()) };
        }
        return RS_switchable_(L"ResetTabNameCommandKey");
    }

    winrt::hstring ExecuteCommandlineArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Run commandline "{_Commandline}" in this window"
        if (!Commandline().empty())
        {
            return winrt::hstring{ RS_switchable_fmt(L"ExecuteCommandlineCommandKey", Commandline()) };
        }
        return {};
    }

    winrt::hstring CloseOtherTabsArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Index())
        {
            // "Close tabs other than index {0}"
            return winrt::hstring{ RS_switchable_fmt(L"CloseOtherTabsCommandKey", Index().Value()) };
        }
        return RS_switchable_(L"CloseOtherTabsDefaultCommandKey");
    }

    winrt::hstring CloseTabsAfterArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Index())
        {
            // "Close tabs after index {0}"
            return winrt::hstring{ RS_switchable_fmt(L"CloseTabsAfterCommandKey", Index().Value()) };
        }
        return RS_switchable_(L"CloseTabsAfterDefaultCommandKey");
    }

    winrt::hstring CloseTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Index())
        {
            // "Close tab at index {0}"
            return winrt::hstring{ RS_switchable_fmt(L"CloseTabAtIndexCommandKey", Index().Value()) };
        }
        return RS_switchable_(L"CloseTabCommandKey");
    }

    winrt::hstring ScrollUpArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (RowsToScroll())
        {
            return winrt::hstring{ RS_switchable_fmt(L"ScrollUpSeveralRowsCommandKey", RowsToScroll().Value()) };
        }
        return RS_switchable_(L"ScrollUpCommandKey");
    }

    winrt::hstring ScrollDownArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (RowsToScroll())
        {
            return winrt::hstring{ RS_switchable_fmt(L"ScrollDownSeveralRowsCommandKey", RowsToScroll().Value()) };
        }
        return RS_switchable_(L"ScrollDownCommandKey");
    }

    winrt::hstring ScrollToMarkArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        switch (Direction())
        {
        case Microsoft::Terminal::Control::ScrollToMarkDirection::Last:
            return winrt::hstring{ RS_switchable_(L"ScrollToLastMarkCommandKey") };
        case Microsoft::Terminal::Control::ScrollToMarkDirection::First:
            return winrt::hstring{ RS_switchable_(L"ScrollToFirstMarkCommandKey") };
        case Microsoft::Terminal::Control::ScrollToMarkDirection::Next:
            return winrt::hstring{ RS_switchable_(L"ScrollToNextMarkCommandKey") };
        case Microsoft::Terminal::Control::ScrollToMarkDirection::Previous:
        default:
            return winrt::hstring{ RS_switchable_(L"ScrollToPreviousMarkCommandKey") };
        }
        return winrt::hstring{ RS_switchable_(L"ScrollToPreviousMarkCommandKey") };
    }

    winrt::hstring AddMarkArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Color())
        {
            return winrt::hstring{ RS_switchable_fmt(L"AddMarkWithColorCommandKey", til::color{ Color().Value() }.ToHexString(true)) };
        }
        else
        {
            return RS_switchable_(L"AddMarkCommandKey");
        }
    }

    winrt::hstring MoveTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (!Window().empty())
        {
            if (Window() == L"new")
            {
                return RS_switchable_(L"MoveTabToNewWindowCommandKey");
            }
            return winrt::hstring{ RS_switchable_fmt(L"MoveTabToWindowCommandKey", Window()) };
        }

        winrt::hstring directionString;
        switch (Direction())
        {
        case MoveTabDirection::Forward:
            directionString = RS_switchable_(L"MoveTabDirectionForward");
            break;
        case MoveTabDirection::Backward:
            directionString = RS_switchable_(L"MoveTabDirectionBackward");
            break;
        }
        return winrt::hstring{ RS_switchable_fmt(L"MoveTabCommandKey", directionString) };
    }

    winrt::hstring ToggleCommandPaletteArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (LaunchMode() == CommandPaletteLaunchMode::CommandLine)
        {
            return RS_switchable_(L"ToggleCommandPaletteCommandLineModeCommandKey");
        }
        return RS_switchable_(L"ToggleCommandPaletteCommandKey");
    }

    winrt::hstring SuggestionsArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        std::wstring str;
        str.append(RS_switchable_(L"SuggestionsCommandKey"));

        if (UseCommandline())
        {
            str.append(L", useCommandline:true");
        }

        // All of the source values will leave a trailing ", " that we need to chop later:
        str.append(L", source: ");
        const auto source = Source();
        if (source == SuggestionsSource::All)
        {
            str.append(L"all, ");
        }
        else if (source == static_cast<SuggestionsSource>(0))
        {
            str.append(L"none, ");
        }
        else
        {
            if (WI_IsFlagSet(source, SuggestionsSource::Tasks))
            {
                str.append(L"tasks, ");
            }

            if (WI_IsFlagSet(source, SuggestionsSource::CommandHistory))
            {
                str.append(L"commandHistory, ");
            }
        }
        // Chop off the last ","
        str.resize(str.size() - 2);
        return winrt::hstring{ str };
    }

    winrt::hstring FindMatchArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        switch (Direction())
        {
        case FindMatchDirection::Next:
            return winrt::hstring{ RS_switchable_(L"FindNextCommandKey") };
        case FindMatchDirection::Previous:
            return winrt::hstring{ RS_switchable_(L"FindPrevCommandKey") };
        }
        return {};
    }

    winrt::hstring NewWindowArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        winrt::hstring newTerminalArgsStr;
        if (ContentArgs())
        {
            newTerminalArgsStr = ContentArgs().GenerateName(context);
        }

        if (newTerminalArgsStr.empty())
        {
            return RS_switchable_(L"NewWindowCommandKey");
        }
        return winrt::hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), RS_switchable_(L"NewWindowCommandKey"), newTerminalArgsStr) };
    }

    winrt::hstring PrevTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (!SwitcherMode())
        {
            return RS_switchable_(L"PrevTabCommandKey");
        }

        const auto mode = SwitcherMode().Value() == TabSwitcherMode::MostRecentlyUsed ? L"most recently used" : L"in order";
        return winrt::hstring(fmt::format(FMT_COMPILE(L"{}, {}"), RS_switchable_(L"PrevTabCommandKey"), mode));
    }

    winrt::hstring NextTabArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (!SwitcherMode())
        {
            return RS_switchable_(L"NextTabCommandKey");
        }

        const auto mode = SwitcherMode().Value() == TabSwitcherMode::MostRecentlyUsed ? L"most recently used" : L"in order";
        return winrt::hstring(fmt::format(FMT_COMPILE(L"{}, {}"), RS_switchable_(L"NextTabCommandKey"), mode));
    }

    winrt::hstring RenameWindowArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Rename window to \"{_Name}\""
        // "Clear window name"
        if (!Name().empty())
        {
            return winrt::hstring{ RS_switchable_fmt(L"RenameWindowCommandKey", Name()) };
        }
        return RS_switchable_(L"ResetWindowNameCommandKey");
    }

    winrt::hstring SearchForTextArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (QueryUrl().empty())
        {
            // Return the default command name, because we'll just use the
            // default search engine for this.
            return RS_switchable_(L"SearchWebCommandKey");
        }

        try
        {
            return winrt::hstring{ RS_switchable_fmt(L"SearchForTextCommandKey", Windows::Foundation::Uri(QueryUrl()).Domain()) };
        }
        CATCH_LOG();

        // We couldn't parse a URL out of this. Return no string at all, so that
        // we don't even put this into the command palette.
        return {};
    }

    winrt::hstring GlobalSummonArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // GH#10210 - Is this action literally the same thing as the `quakeMode`
        // action? That has a special name.
        static const auto quakeModeArgs{ std::get<0>(GlobalSummonArgs::QuakeModeFromJson(Json::Value::null)) };
        if (quakeModeArgs.Equals(*this))
        {
            return RS_switchable_(L"QuakeModeCommandKey");
        }

        std::wstring str{ RS_switchable_(L"GlobalSummonCommandKey") };

        // "Summon the Terminal window"
        // "Summon the Terminal window, name:\"{_Name}\""
        if (!Name().empty())
        {
            str.append(L", name: ");
            str.append(Name());
        }
        return winrt::hstring{ str };
    }

    winrt::hstring FocusPaneArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Focus pane {Id}"
        return winrt::hstring{ RS_switchable_fmt(L"FocusPaneCommandKey", Id()) };
    }

    winrt::hstring ExportBufferArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (!Path().empty())
        {
            // "Export text to {path}"
            return winrt::hstring{ RS_switchable_fmt(L"ExportBufferToPathCommandKey", Path()) };
        }
        else
        {
            // "Export text"
            return RS_switchable_(L"ExportBufferCommandKey");
        }
    }

    winrt::hstring ClearBufferArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        // "Clear Buffer"
        // "Clear Viewport"
        // "Clear Scrollback"
        switch (Clear())
        {
        case Control::ClearBufferType::All:
            return RS_switchable_(L"ClearAllCommandKey");
        case Control::ClearBufferType::Screen:
            return RS_switchable_(L"ClearViewportCommandKey");
        case Control::ClearBufferType::Scrollback:
            return RS_switchable_(L"ClearScrollbackCommandKey");
        }

        // Return the empty string - the Clear() should be one of these values
        return {};
    }

    winrt::hstring MultipleActionsArgs::GenerateName(const winrt::WARC::ResourceContext&) const
    {
        return {};
    }

    winrt::hstring AdjustOpacityArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Relative())
        {
            if (Opacity() >= 0)
            {
                // "Increase background opacity by {Opacity}%"
                return winrt::hstring{ RS_switchable_fmt(L"IncreaseOpacityCommandKey", Opacity()) };
            }
            else
            {
                // "Decrease background opacity by {Opacity}%"
                return winrt::hstring{ RS_switchable_fmt(L"DecreaseOpacityCommandKey", Opacity()) };
            }
        }
        else
        {
            // "Set background opacity to {Opacity}%"
            return winrt::hstring{ RS_switchable_fmt(L"AdjustOpacityCommandKey", Opacity()) };
        }
    }

    winrt::hstring SaveSnippetArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        if (Feature_SaveSnippet::IsEnabled())
        {
            auto str = fmt::format(FMT_COMPILE(L"{} commandline: {}"), RS_switchable_(L"SaveSnippetNamePrefix"), Commandline());

            if (!Name().empty())
            {
                fmt::format_to(std::back_inserter(str), FMT_COMPILE(L", name: {}"), Name());
            }

            if (!KeyChord().empty())
            {
                fmt::format_to(std::back_inserter(str), FMT_COMPILE(L", keyChord {}"), KeyChord());
            }

            return winrt::hstring{ str };
        }
        return {};
    }

    static winrt::hstring _FormatColorString(const Control::SelectionColor& selectionColor)
    {
        if (!selectionColor)
        {
            return RS_(L"ColorSelection_defaultColor");
        }

        const auto color = selectionColor.Color();
        const auto isIndexed16 = selectionColor.IsIndex16();
        winrt::hstring colorStr;

        if (isIndexed16)
        {
            static const std::array indexedColorNames{
                USES_RESOURCE(L"ColorSelection_Black"),
                USES_RESOURCE(L"ColorSelection_Red"),
                USES_RESOURCE(L"ColorSelection_Green"),
                USES_RESOURCE(L"ColorSelection_Yellow"),
                USES_RESOURCE(L"ColorSelection_Blue"),
                USES_RESOURCE(L"ColorSelection_Purple"),
                USES_RESOURCE(L"ColorSelection_Cyan"),
                USES_RESOURCE(L"ColorSelection_White"),
                USES_RESOURCE(L"ColorSelection_BrightBlack"),
                USES_RESOURCE(L"ColorSelection_BrightRed"),
                USES_RESOURCE(L"ColorSelection_BrightGreen"),
                USES_RESOURCE(L"ColorSelection_BrightYellow"),
                USES_RESOURCE(L"ColorSelection_BrightBlue"),
                USES_RESOURCE(L"ColorSelection_BrightPurple"),
                USES_RESOURCE(L"ColorSelection_BrightCyan"),
                USES_RESOURCE(L"ColorSelection_BrightWhite"),
            };
            static_assert(indexedColorNames.size() == 16);

            if (color.R < indexedColorNames.size())
            {
                colorStr = GetLibraryResourceString(til::at(indexedColorNames, color.R));
            }
            else
            {
                wchar_t tempBuf[9] = { 0 };
                swprintf_s(tempBuf, L"i%02i", color.R);
                colorStr = tempBuf;
            }
        }
        else
        {
            colorStr = til::color{ color }.ToHexString(true);
        }

        return colorStr;
    }

    static bool _isBoringColor(const Control::SelectionColor& selectionColor)
    {
        if (!selectionColor)
        {
            return true;
        }

        const til::color color{ selectionColor.Color() };
        return color.with_alpha(0) == til::color{};
    }

    winrt::hstring ColorSelectionArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        auto matchModeStr = winrt::hstring{};
        if (MatchMode() == Core::MatchMode::All)
        {
            matchModeStr = fmt::format(FMT_COMPILE(L", {}"), RS_switchable_(L"ColorSelection_allMatches")); // ", all matches"
        }

        const auto foreground = Foreground();
        const auto background = Background();
        const auto fgStr = _FormatColorString(foreground);
        const auto bgStr = _FormatColorString(background);

        // To try to keep things simple for the user, we'll try to show only the
        // "interesting" color (i.e. leave off the bg or fg if it is either unspecified or
        // black or index 0).
        //
        // Note that we mask off the alpha channel, which is used to indicate if it's an
        // indexed color.
        const auto foregroundIsBoring = _isBoringColor(foreground);
        const auto backgroundIsBoring = _isBoringColor(background);

        if (foreground && backgroundIsBoring)
        {
            // "Color selection, foreground: {0}{1}"
            return winrt::hstring{ RS_switchable_fmt(L"ColorSelection_fg_action", fgStr, matchModeStr) };
        }
        else if (background && foregroundIsBoring)
        {
            // "Color selection, background: {0}{1}"
            return winrt::hstring{ RS_switchable_fmt(L"ColorSelection_bg_action", bgStr, matchModeStr) };
        }
        else if (foreground && background)
        {
            // "Color selection, foreground: {0}, background: {1}{2}"
            return winrt::hstring{ RS_switchable_fmt(L"ColorSelection_fg_bg_action", fgStr, bgStr, matchModeStr) };
        }
        else
        {
            // "Color selection, (default foreground/background){0}"
            return winrt::hstring{ RS_switchable_fmt(L"ColorSelection_default_action", matchModeStr) };
        }
    }

    winrt::hstring SelectOutputArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        switch (Direction())
        {
        case SelectOutputDirection::Next:
            return RS_switchable_(L"SelectOutputNextCommandKey");
        case SelectOutputDirection::Previous:
            return RS_switchable_(L"SelectOutputPreviousCommandKey");
        }
        return {};
    }
    winrt::hstring SelectCommandArgs::GenerateName(const winrt::WARC::ResourceContext& context) const
    {
        switch (Direction())
        {
        case SelectOutputDirection::Next:
            return RS_switchable_(L"SelectCommandNextCommandKey");
        case SelectOutputDirection::Previous:
            return RS_switchable_(L"SelectCommandPreviousCommandKey");
        }
        return {};
    }
}
