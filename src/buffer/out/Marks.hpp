/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- marks.hpp

Abstract:
- Definitions for types that are used for "scroll marks" and shell integration
  in the buffer.
- Scroll marks are identified by the existence of "ScrollbarData" on a ROW in the buffer.
- Shell integration will then also markup the buffer with special
  TextAttributes, to identify regions of text as the Prompt, the Command, the
  Output, etc.
- MarkExtents are used to abstract away those regions of text, so a caller
  doesn't need to iterate over the buffer themselves.
--*/

#pragma once

enum class MarkCategory : uint8_t
{
    Default = 0,
    Error = 1,
    Warning = 2,
    Success = 3,
    Prompt = 4
};

// This is the data that's stored on each ROW, to suggest that there's something
// interesting on this row to show in the scrollbar. Also used in conjunction
// with shell integration - when a prompt is added through shell integration,
// we'll also add a scrollbar mark as a quick "bookmark" to the start of that
// command.
struct ScrollbarData
{
    MarkCategory category{ MarkCategory::Default };

    // Scrollbar marks may have been given a color, or not.
    std::optional<til::color> color;

    // Prompts without an exit code haven't had a matching FTCS CommandEnd
    // called yet. Any value other than 0 is an error.
    std::optional<uint32_t> exitCode;
    // Future consideration: stick the literal command as a string on here, if
    // we were given it with the 633;E sequence.
};

// Helper struct for describing the bounds of a command and it's output,
// * The Prompt is between the start & end
// * The Command is between the end & commandEnd
// * The Output is between the commandEnd & outputEnd
//
// These are not actually stored in the buffer. The buffer can produce them for
// callers, to make reasoning about regions of the buffer easier.
struct MarkExtents
{
    // Data from the row
    ScrollbarData data;

    til::point start;
    til::point end; // exclusive
    std::optional<til::point> commandEnd;
    std::optional<til::point> outputEnd;

    // MarkCategory category{ MarkCategory::Info };
    // Other things we may want to think about in the future are listed in
    // GH#11000

    bool HasCommand() const noexcept
    {
        return commandEnd.has_value() && *commandEnd != end;
    }
    bool HasOutput() const noexcept
    {
        return outputEnd.has_value() && *outputEnd != *commandEnd;
    }
    std::pair<til::point, til::point> GetExtent() const
    {
        til::point realEnd{ til::coalesce_value(outputEnd, commandEnd, end) };
        return std::make_pair(start, realEnd);
    }
};

// Another helper, for when callers would like to know just about the data of
// the scrollbar, but don't actually need all the extents of prompts.
struct ScrollMark
{
    til::CoordType row{ 0 };
    ScrollbarData data;
};
