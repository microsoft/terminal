/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderTarget.hpp

Abstract:
- This serves as the entry point for console rendering activities.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "FontInfoDesired.hpp"
#include "IRenderEngine.hpp"
#include "../types/inc/viewport.hpp"

namespace Microsoft::Console::Render
{
    class IRenderTarget
    {
    public:
        virtual ~IRenderTarget() = 0;

    protected:
        IRenderTarget() = default;
        IRenderTarget(const IRenderTarget&) = default;
        IRenderTarget(IRenderTarget&&) = default;
        IRenderTarget& operator=(const IRenderTarget&) = default;
        IRenderTarget& operator=(IRenderTarget&&) = default;

    public:
        virtual void TriggerRedraw(const Microsoft::Console::Types::Viewport& region) = 0;
        virtual void TriggerRedraw(const COORD* const pcoord) = 0;
        virtual void TriggerRedrawCursor(const COORD* const pcoord) = 0;

        virtual void TriggerRedrawAll() = 0;
        virtual void TriggerTeardown() noexcept = 0;

        virtual void TriggerSelection() = 0;
        virtual void TriggerScroll() = 0;
        virtual void TriggerScroll(const COORD* const pcoordDelta) = 0;
        virtual void TriggerCircling() = 0;
        virtual void TriggerTitleChange() = 0;
    };

    inline Microsoft::Console::Render::IRenderTarget::~IRenderTarget() {}

}
