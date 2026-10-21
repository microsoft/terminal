// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Grammar for the `wt --window,-w <target>` argument, shared by the commandline
// dispatch path (WindowEmperor::_dispatchCommandline) and the Terminal Protocol
// server (TerminalProtocolComServer).

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
    // "new", or any negative integer. Always a brand new window.
    New,
    // Anything else, including "_quake": the window with that WindowName().
    Name,
};

struct WindowTarget
{
    WindowTargetKind Kind{ WindowTargetKind::Unspecified };
    // Only relevant when Kind == Id.
    uint64_t Id{ 0 };
    // Only relevant when Kind == Name.
    winrt::hstring Name;
};

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
        // Negative IDs mean a new window
        return { WindowTargetKind::New };
    }

    if (target == L"last")
    {
        return { WindowTargetKind::Existing };
    }
    if (target == L"new")
    {
        return { WindowTargetKind::New };
    }

    return { WindowTargetKind::Name, 0, target };
}
