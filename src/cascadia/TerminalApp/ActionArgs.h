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

// Notes on defining EventArgs:
// For EventArgs that require actual args (ex: NewTabWithProfileEventArgs)
// - The first ctor is the projected ctor, the one that's required by cppwinrt.
//   It requires the projection-namespaced IActionArgs type.
// - the second is internal to the TerminalApp implementation. It uses the
//   implementation type of the IActionArgs. This lets us construct the args
//   without getting the activation factory.

namespace winrt::TerminalApp::implementation
{
    struct CopyTextArgs : public CopyTextArgsT<CopyTextArgs>
    {
        CopyTextArgs() = default;
        GETSET_PROPERTY(bool, TrimWhitespace, false);
    };
    struct CopyTextEventArgs : public CopyTextEventArgsT<CopyTextEventArgs, TerminalApp::implementation::CopyTextArgs>
    {
        CopyTextEventArgs(const CopyTextArgs& args) :
            _TrimWhitespace{ args.TrimWhitespace() } {};

        GETSET_PROPERTY(bool, Handled, false);
        GETSET_PROPERTY(bool, TrimWhitespace, false);
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
        // NewTabWithProfileEventArgs(const TerminalApp::NewTabWithProfileArgs& args) :
        //     _ProfileIndex{ args.ProfileIndex() } {};

        NewTabWithProfileEventArgs(const NewTabWithProfileArgs& args)//  :
            //NewTabWithProfileArgs(args.ProfileIndex())
            // _ProfileIndex{ args.ProfileIndex() }
        {
            ProfileIndex(args.ProfileIndex());
            TerminalApp::NewTabWithProfileArgs self{ *this };
            self.ProfileIndex(args.ProfileIndex());
            // this->try_as<NewTabEventArgs>().Pro
        };

        GETSET_PROPERTY(bool, Handled, false);

        // GETSET_PROPERTY(int32_t, ProfileIndex, 0);
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
        SwitchToTabEventArgs(const SwitchToTabArgs& args) :
            _TabIndex{ args.TabIndex() } {};
        GETSET_PROPERTY(bool, Handled, false);
        GETSET_PROPERTY(int32_t, TabIndex, 0);
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
        ResizePaneEventArgs(const ResizePaneArgs& args) :
            _Direction{ args.Direction() } {};

        GETSET_PROPERTY(bool, Handled, false);
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::Left);
    };

    struct MoveFocusArgs : public MoveFocusArgsT<MoveFocusArgs>
    {
        MoveFocusArgs() = default;
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::Left);
    };
    struct MoveFocusEventArgs : public MoveFocusEventArgsT<MoveFocusEventArgs, TerminalApp::implementation::MoveFocusArgs>
    {
        MoveFocusEventArgs(const MoveFocusArgs& args) :
            _Direction{ args.Direction() } {};

        GETSET_PROPERTY(bool, Handled, false);
        GETSET_PROPERTY(TerminalApp::Direction, Direction, TerminalApp::Direction::Left);
    };

}

namespace winrt::TerminalApp::factory_implementation
{
    // BASIC_FACTORY(PasteTextEventArgs);
    // BASIC_FACTORY(NewTabEventArgs);
    // BASIC_FACTORY(DuplicateTabEventArgs);
    // BASIC_FACTORY(NewTabWithProfileEventArgs);
    // BASIC_FACTORY(NewWindowEventArgs);
    // BASIC_FACTORY(CloseWindowEventArgs);
    // BASIC_FACTORY(CloseTabEventArgs);
    // BASIC_FACTORY(ClosePaneEventArgs);
    // BASIC_FACTORY(SwitchToTabEventArgs);
    // BASIC_FACTORY(NextTabEventArgs);
    // BASIC_FACTORY(PrevTabEventArgs);
    // BASIC_FACTORY(SplitVerticalEventArgs);
    // BASIC_FACTORY(SplitHorizontalEventArgs);
    // BASIC_FACTORY(IncreaseFontSizeEventArgs);
    // BASIC_FACTORY(DecreaseFontSizeEventArgs);
    // BASIC_FACTORY(ScrollUpEventArgs);
    // BASIC_FACTORY(ScrollDownEventArgs);
    // BASIC_FACTORY(ScrollUpPageEventArgs);
    // BASIC_FACTORY(ScrollDownPageEventArgs);
    // BASIC_FACTORY(OpenSettingsEventArgs);
    // BASIC_FACTORY(ResizePaneEventArgs);
    // BASIC_FACTORY(MoveFocusEventArgs);
}
