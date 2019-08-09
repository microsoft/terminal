// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CopyTextEventArgs.g.h"

#include "CopyTextEventArgs.g.h"
#include "PasteTextEventArgs.g.h"
#include "NewTabEventArgs.g.h"
#include "DuplicateTabEventArgs.g.h"
#include "NewTabWithProfileEventArgs.g.h"
#include "NewWindowEventArgs.g.h"
#include "CloseWindowEventArgs.g.h"
#include "CloseTabEventArgs.g.h"
#include "ClosePaneEventArgs.g.h"
#include "NextTabEventArgs.g.h"
#include "PrevTabEventArgs.g.h"
#include "SplitVerticalEventArgs.g.h"
#include "SplitHorizontalEventArgs.g.h"
#include "SwitchToTabEventArgs.g.h"
#include "IncreaseFontSizeEventArgs.g.h"
#include "DecreaseFontSizeEventArgs.g.h"
#include "ScrollUpEventArgs.g.h"
#include "ScrollDownEventArgs.g.h"
#include "ScrollUpPageEventArgs.g.h"
#include "ScrollDownPageEventArgs.g.h"
#include "OpenSettingsEventArgs.g.h"
#include "ResizePaneEventArgs.g.h"
#include "MoveFocusEventArgs.g.h"

#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct CopyTextEventArgs : public CopyTextEventArgsT<CopyTextEventArgs>
    {
        CopyTextEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
        GETSET_PROPERTY(bool, TrimWhitespace);
    };

    struct PasteTextEventArgs : public PasteTextEventArgsT<PasteTextEventArgs>
    {
        PasteTextEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct NewTabEventArgs : public NewTabEventArgsT<NewTabEventArgs>
    {
        NewTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct DuplicateTabEventArgs : public DuplicateTabEventArgsT<DuplicateTabEventArgs>
    {
        DuplicateTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct NewTabWithProfileEventArgs : public NewTabWithProfileEventArgsT<NewTabWithProfileEventArgs>
    {
        NewTabWithProfileEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
        GETSET_PROPERTY(int32_t, ProfileIndex);
    };

    struct NewWindowEventArgs : public NewWindowEventArgsT<NewWindowEventArgs>
    {
        NewWindowEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct CloseWindowEventArgs : public CloseWindowEventArgsT<CloseWindowEventArgs>
    {
        CloseWindowEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct CloseTabEventArgs : public CloseTabEventArgsT<CloseTabEventArgs>
    {
        CloseTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ClosePaneEventArgs : public ClosePaneEventArgsT<ClosePaneEventArgs>
    {
        ClosePaneEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct NextTabEventArgs : public NextTabEventArgsT<NextTabEventArgs>
    {
        NextTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct PrevTabEventArgs : public PrevTabEventArgsT<PrevTabEventArgs>
    {
        PrevTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct SplitVerticalEventArgs : public SplitVerticalEventArgsT<SplitVerticalEventArgs>
    {
        SplitVerticalEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct SplitHorizontalEventArgs : public SplitHorizontalEventArgsT<SplitHorizontalEventArgs>
    {
        SplitHorizontalEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct SwitchToTabEventArgs : public SwitchToTabEventArgsT<SwitchToTabEventArgs>
    {
        SwitchToTabEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
        GETSET_PROPERTY(int32_t, TabIndex);
    };

    struct IncreaseFontSizeEventArgs : public IncreaseFontSizeEventArgsT<IncreaseFontSizeEventArgs>
    {
        IncreaseFontSizeEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct DecreaseFontSizeEventArgs : public DecreaseFontSizeEventArgsT<DecreaseFontSizeEventArgs>
    {
        DecreaseFontSizeEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ScrollUpEventArgs : public ScrollUpEventArgsT<ScrollUpEventArgs>
    {
        ScrollUpEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ScrollDownEventArgs : public ScrollDownEventArgsT<ScrollDownEventArgs>
    {
        ScrollDownEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ScrollUpPageEventArgs : public ScrollUpPageEventArgsT<ScrollUpPageEventArgs>
    {
        ScrollUpPageEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ScrollDownPageEventArgs : public ScrollDownPageEventArgsT<ScrollDownPageEventArgs>
    {
        ScrollDownPageEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct OpenSettingsEventArgs : public OpenSettingsEventArgsT<OpenSettingsEventArgs>
    {
        OpenSettingsEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
    };

    struct ResizePaneEventArgs : public ResizePaneEventArgsT<ResizePaneEventArgs>
    {
        ResizePaneEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
        GETSET_PROPERTY(TerminalApp::Direction, Direction);
    };

    struct MoveFocusEventArgs : public MoveFocusEventArgsT<MoveFocusEventArgs>
    {
        MoveFocusEventArgs() = default;
        GETSET_PROPERTY(bool, Handled);
        GETSET_PROPERTY(TerminalApp::Direction, Direction);
    };

}

namespace winrt::TerminalApp::factory_implementation
{
    struct CopyTextEventArgs : CopyTextEventArgsT<CopyTextEventArgs, implementation::CopyTextEventArgs>
    {
    };
    struct PasteTextEventArgs : PasteTextEventArgsT<PasteTextEventArgs, implementation::PasteTextEventArgs>
    {
    };
    struct NewTabEventArgs : NewTabEventArgsT<NewTabEventArgs, implementation::NewTabEventArgs>
    {
    };
    struct DuplicateTabEventArgs : DuplicateTabEventArgsT<DuplicateTabEventArgs, implementation::DuplicateTabEventArgs>
    {
    };
    struct NewTabWithProfileEventArgs : NewTabWithProfileEventArgsT<NewTabWithProfileEventArgs, implementation::NewTabWithProfileEventArgs>
    {
    };
    struct NewWindowEventArgs : NewWindowEventArgsT<NewWindowEventArgs, implementation::NewWindowEventArgs>
    {
    };
    struct CloseWindowEventArgs : CloseWindowEventArgsT<CloseWindowEventArgs, implementation::CloseWindowEventArgs>
    {
    };
    struct CloseTabEventArgs : CloseTabEventArgsT<CloseTabEventArgs, implementation::CloseTabEventArgs>
    {
    };
    struct ClosePaneEventArgs : ClosePaneEventArgsT<ClosePaneEventArgs, implementation::ClosePaneEventArgs>
    {
    };
    struct SwitchToTabEventArgs : SwitchToTabEventArgsT<SwitchToTabEventArgs, implementation::SwitchToTabEventArgs>
    {
    };
    struct NextTabEventArgs : NextTabEventArgsT<NextTabEventArgs, implementation::NextTabEventArgs>
    {
    };
    struct PrevTabEventArgs : PrevTabEventArgsT<PrevTabEventArgs, implementation::PrevTabEventArgs>
    {
    };
    struct SplitVerticalEventArgs : SplitVerticalEventArgsT<SplitVerticalEventArgs, implementation::SplitVerticalEventArgs>
    {
    };
    struct SplitHorizontalEventArgs : SplitHorizontalEventArgsT<SplitHorizontalEventArgs, implementation::SplitHorizontalEventArgs>
    {
    };
    struct IncreaseFontSizeEventArgs : IncreaseFontSizeEventArgsT<IncreaseFontSizeEventArgs, implementation::IncreaseFontSizeEventArgs>
    {
    };
    struct DecreaseFontSizeEventArgs : DecreaseFontSizeEventArgsT<DecreaseFontSizeEventArgs, implementation::DecreaseFontSizeEventArgs>
    {
    };
    struct ScrollUpEventArgs : ScrollUpEventArgsT<ScrollUpEventArgs, implementation::ScrollUpEventArgs>
    {
    };
    struct ScrollDownEventArgs : ScrollDownEventArgsT<ScrollDownEventArgs, implementation::ScrollDownEventArgs>
    {
    };
    struct ScrollUpPageEventArgs : ScrollUpPageEventArgsT<ScrollUpPageEventArgs, implementation::ScrollUpPageEventArgs>
    {
    };
    struct ScrollDownPageEventArgs : ScrollDownPageEventArgsT<ScrollDownPageEventArgs, implementation::ScrollDownPageEventArgs>
    {
    };
    struct OpenSettingsEventArgs : OpenSettingsEventArgsT<OpenSettingsEventArgs, implementation::OpenSettingsEventArgs>
    {
    };
    struct ResizePaneEventArgs : ResizePaneEventArgsT<ResizePaneEventArgs, implementation::ResizePaneEventArgs>
    {
    };
    struct MoveFocusEventArgs : MoveFocusEventArgsT<MoveFocusEventArgs, implementation::MoveFocusEventArgs>
    {
    };
}
