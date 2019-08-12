// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// HEY YOU: When adding ActionArgs types, make sure to add the corresponding
//          *.g.cpp to ActionArgs.cpp!
#include "CopyTextArgs.g.h"
#include "CopyTextEventArgs.g.h"
#include "PasteTextEventArgs.g.h"
#include "NewTabEventArgs.g.h"
#include "DuplicateTabEventArgs.g.h"
#include "NewTabWithProfileArgs.g.h"
#include "NewTabWithProfileEventArgs.g.h"
#include "NewWindowEventArgs.g.h"
#include "CloseWindowEventArgs.g.h"
#include "CloseTabEventArgs.g.h"
#include "ClosePaneEventArgs.g.h"
#include "NextTabEventArgs.g.h"
#include "PrevTabEventArgs.g.h"
#include "SplitVerticalEventArgs.g.h"
#include "SplitHorizontalEventArgs.g.h"
#include "SwitchToTabArgs.g.h"
#include "SwitchToTabEventArgs.g.h"
#include "IncreaseFontSizeEventArgs.g.h"
#include "DecreaseFontSizeEventArgs.g.h"
#include "ScrollUpEventArgs.g.h"
#include "ScrollDownEventArgs.g.h"
#include "ScrollUpPageEventArgs.g.h"
#include "ScrollDownPageEventArgs.g.h"
#include "OpenSettingsEventArgs.g.h"
#include "ResizePaneArgs.g.h"
#include "ResizePaneEventArgs.g.h"
#include "MoveFocusArgs.g.h"
#include "MoveFocusEventArgs.g.h"

#include "../../cascadia/inc/cppwinrt_utils.h"

// Notes on defining ActionArgs and ActionEventArgs:
// * All properties specific to an action should be defined as in the ActionArgs
//   class.
// * ActionEventArgs should extend from the ActionArgs class, and add an
//   aditional `Handled` property. For EventArgs that require actual args (ex:
//   NewTabWithProfileEventArgs)
// * For ActionEventArgs:
//   - the ctor is internal to the TerminalApp implementation. It uses the
//     implementation type of the IActionArgs. This lets us construct the args
//     without getting the activation factory.
//   - You need to manually set each member of the EventArgs using the Args
//     that's passed in. DO NOT declare the properties a second time in the
//     event! Because the EventArgs class uses the Args class as a base class,
//     the EventArgs already has methods for each property defined.

namespace winrt::TerminalApp::implementation
{
    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        GETSET_PROPERTY(bool, TrimWhitespace, false);
    };
    struct CopyTextEventArgs : public CopyTextEventArgsT<CopyTextEventArgs, TerminalApp::implementation::CopyTextArgs>
    {
        CopyTextEventArgs(const CopyTextArgs& args)
        {
            TrimWhitespace(args.TrimWhitespace());
        };
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct PasteTextEventArgs : public PasteTextEventArgsT<PasteTextEventArgs>
    {
        PasteTextEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct NewTabEventArgs : public NewTabEventArgsT<NewTabEventArgs>
    {
        NewTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct DuplicateTabEventArgs : public DuplicateTabEventArgsT<DuplicateTabEventArgs>
    {
        DuplicateTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct NewTabWithProfileArgs : public NewTabWithProfileArgsT<NewTabWithProfileArgs>
    {
        NewTabWithProfileArgs() = default;
        GETSET_PROPERTY(int32_t, ProfileIndex, 0);
    };

    struct NewTabWithProfileEventArgs : public NewTabWithProfileEventArgsT<NewTabWithProfileEventArgs, TerminalApp::implementation::NewTabWithProfileArgs>
    {
        NewTabWithProfileEventArgs(const NewTabWithProfileArgs& args)
        {
            ProfileIndex(args.ProfileIndex());
        };
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct NewWindowEventArgs : public NewWindowEventArgsT<NewWindowEventArgs>
    {
        NewWindowEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct CloseWindowEventArgs : public CloseWindowEventArgsT<CloseWindowEventArgs>
    {
        CloseWindowEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct CloseTabEventArgs : public CloseTabEventArgsT<CloseTabEventArgs>
    {
        CloseTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ClosePaneEventArgs : public ClosePaneEventArgsT<ClosePaneEventArgs>
    {
        ClosePaneEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct NextTabEventArgs : public NextTabEventArgsT<NextTabEventArgs>
    {
        NextTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct PrevTabEventArgs : public PrevTabEventArgsT<PrevTabEventArgs>
    {
        PrevTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct SplitVerticalEventArgs : public SplitVerticalEventArgsT<SplitVerticalEventArgs>
    {
        SplitVerticalEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct SplitHorizontalEventArgs : public SplitHorizontalEventArgsT<SplitHorizontalEventArgs>
    {
        SplitHorizontalEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct SwitchToTabArgs : public SwitchToTabArgsT<SwitchToTabArgs>
    {
        SwitchToTabArgs() = default;
        GETSET_PROPERTY(int32_t, TabIndex, 0);
    };
    struct SwitchToTabEventArgs : public SwitchToTabEventArgsT<SwitchToTabEventArgs, TerminalApp::implementation::SwitchToTabArgs>
    {
        SwitchToTabEventArgs(const SwitchToTabArgs& args)
        {
            TabIndex(args.TabIndex());
        };
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct IncreaseFontSizeEventArgs : public IncreaseFontSizeEventArgsT<IncreaseFontSizeEventArgs>
    {
        IncreaseFontSizeEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct DecreaseFontSizeEventArgs : public DecreaseFontSizeEventArgsT<DecreaseFontSizeEventArgs>
    {
        DecreaseFontSizeEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ScrollUpEventArgs : public ScrollUpEventArgsT<ScrollUpEventArgs>
    {
        ScrollUpEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ScrollDownEventArgs : public ScrollDownEventArgsT<ScrollDownEventArgs>
    {
        ScrollDownEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ScrollUpPageEventArgs : public ScrollUpPageEventArgsT<ScrollUpPageEventArgs>
    {
        ScrollUpPageEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ScrollDownPageEventArgs : public ScrollDownPageEventArgsT<ScrollDownPageEventArgs>
    {
        ScrollDownPageEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct OpenSettingsEventArgs : public OpenSettingsEventArgsT<OpenSettingsEventArgs>
    {
        OpenSettingsEventArgs() = default;
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct ResizePaneArgs : public ResizePaneArgsT<ResizePaneArgs>
    {
        ResizePaneArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::Left);
    };
    struct ResizePaneEventArgs : public ResizePaneEventArgsT<ResizePaneEventArgs, TerminalApp::implementation::ResizePaneArgs>
    {
        ResizePaneEventArgs(const ResizePaneArgs& args)
        {
            Direction(args.Direction());
        };
        GETSET_PROPERTY(bool, Handled, false);
    };

    struct MoveFocusArgs : public MoveFocusArgsT<MoveFocusArgs>
    {
        MoveFocusArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::Left);
    };
    struct MoveFocusEventArgs : public MoveFocusEventArgsT<MoveFocusEventArgs, TerminalApp::implementation::MoveFocusArgs>
    {
        MoveFocusEventArgs(const MoveFocusArgs& args)
        {
            Direction(args.Direction());
        };
        GETSET_PROPERTY(bool, Handled, false);
    };

}

namespace winrt::TerminalApp::factory_implementation
{
}
