// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Extracted pure-parsing functions from the Terminal Protocol server layer
// for fuzzing and testability. These functions have no COM, WinRT, or XAML
// dependencies and can be called from a LibFuzzer harness.

#pragma once

#include <string>

namespace Microsoft::Terminal::Protocol::Parsing
{
    // ── SplitPane direction mapping ──

    // Mirror of TerminalSettingsModel::SplitDirection enum values.
    // Kept in sync with ActionArgs.idl.
    enum class SplitDirection
    {
        Automatic = 0,
        Up = 1,
        Right = 2,
        Down = 3,
        Left = 4
    };

    // Map a direction string to a SplitDirection value.
    // Accepts: "right", "left", "up", "down", "auto", "automatic",
    // and legacy values "horizontal" (→ Down) / "vertical" (→ Right).
    // Returns Right for unrecognized strings (matching server default).
    inline SplitDirection ParseSplitDirection(const std::string& direction)
    {
        if (direction.empty())
        {
            return SplitDirection::Right;
        }

        if (direction == "right")
        {
            return SplitDirection::Right;
        }
        if (direction == "left")
        {
            return SplitDirection::Left;
        }
        if (direction == "up")
        {
            return SplitDirection::Up;
        }
        if (direction == "down")
        {
            return SplitDirection::Down;
        }
        if (direction == "auto" || direction == "automatic")
        {
            return SplitDirection::Automatic;
        }
        if (direction == "horizontal")
        {
            return SplitDirection::Down;
        }
        if (direction == "vertical")
        {
            return SplitDirection::Right;
        }

        // Unrecognized — default to Right
        return SplitDirection::Right;
    }

    // ── ReadPaneOutput source routing ──

    enum class PaneOutputSource
    {
        Scrollback,
        Screen,
        LastPrompt
    };

    // Classify the source parameter for ReadPaneOutput.
    inline PaneOutputSource ClassifyPaneOutputSource(const std::string& source)
    {
        if (source == "last_prompt")
        {
            return PaneOutputSource::LastPrompt;
        }
        if (source == "screen")
        {
            return PaneOutputSource::Screen;
        }
        return PaneOutputSource::Scrollback;
    }
}
