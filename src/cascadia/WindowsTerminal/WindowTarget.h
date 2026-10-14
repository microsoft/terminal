// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Grammar for the `wt --window,-w <target>` argument, shared by the commandline
// dispatch path (WindowEmperor::_dispatchCommandline) and the Terminal Protocol
// server (TerminalProtocolComServer). Keeping both surfaces on this one parser
// is what stops them from drifting apart.
//
// This classifies the target string and nothing else. It deliberately does NOT
// decide what an unspecified target means: the commandline path resolves that
// from the user's `windowingBehavior` global, while the protocol server
// resolves it to the most recent window.

#pragma once

#include "pch.h"

enum class WindowTargetKind
{
    // "". The caller picks the default.
    Unspecified,
    // A positive integer: the window with that WindowId().
    Id,
    // "0", "-0", or "last": an already-existing window.
    Existing,
    // "new", or any negative integer. Always a brand new window, and never
    // influenced by the `windowingBehavior` global.
    New,
    // Anything else, including "_quake": the window with that WindowName().
    Name,
};

struct WindowTarget
{
    WindowTargetKind Kind{ WindowTargetKind::Unspecified };
    // Only meaningful when Kind == Id.
    uint64_t Id{ 0 };
    // Only meaningful when Kind == Name.
    winrt::hstring Name;
};

// Classify a `-w` target string.
//
// The numeric parse is til::parse_signed<int64_t> with an explicit base of 10,
// which is exactly what the commandline path has always used. That precise call
// is load bearing -- reimplementing it would silently reclassify:
//   "+5"                   -> Name, not Id 5     (a leading '+' is rejected)
//   "0x10"                 -> Name, not Id 16    (base is pinned to 10)
//   "0123"                 -> Id 123, not Id 83  (base is pinned to 10)
//   "-0"                   -> Existing, not New  (sign stripped, then 0 == 0)
//   "1a", " 1"             -> Name, not Id 1     (the whole string must parse)
//   "10000000000000000000" -> Name               (fits uint64, overflows int64)
inline WindowTarget ParseWindowTarget(const winrt::hstring& target)
{
    if (target.empty())
    {
        return { WindowTargetKind::Unspecified };
    }

    if (const auto opt = til::parse_signed<int64_t>(target, 10))
    {
        if (*opt > 0)
        {
            return { WindowTargetKind::Id, gsl::narrow_cast<uint64_t>(*opt) };
        }
        if (*opt == 0)
        {
            return { WindowTargetKind::Existing };
        }
        // Negative IDs mean a new window. In the original if/else chain this
        // case matched no arm at all, and relied on WindowingMode::UseNew being
        // the initial value of the local it was assigning to.
        return { WindowTargetKind::New };
    }

    if (target == L"last")
    {
        return { WindowTargetKind::Existing };
    }
    // "new" likewise matched no arm in the original chain.
    if (target == L"new")
    {
        return { WindowTargetKind::New };
    }

    return { WindowTargetKind::Name, 0, target };
}
