/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ScreenBufferRenderTarget.hpp

Abstract:
Provides an encapsulation for all of the RenderTarget methods for the SCreenBuffer.
Unfortunately, these cannot be defined directly on the SCREEN_INFORMATION due to
MSFT 9358743.
Adding an interface to SCREEN_INFORMATION makes the ConsoleObjectHeader no
longer the first part of the SCREEN_INFORMATION.
The Screen buffer will pass this object to other objects that need to trigger
redrawing the buffer contents.

Author(s):
- Mike Griese (migrie) Nov 2018
--*/

#pragma once
#include "../renderer/inc/IRenderTarget.hpp"

// fwdecl
class SCREEN_INFORMATION;

class ScreenBufferRenderTarget final : public Microsoft::Console::Render::IRenderTarget
{
public:
    ScreenBufferRenderTarget(SCREEN_INFORMATION& owner);

    void TriggerRedraw(const Microsoft::Console::Types::Viewport& region) override;
    void TriggerRedraw(const COORD* const pcoord) override;
    void TriggerRedrawCursor(const COORD* const pcoord) override;
    void TriggerRedrawAll() override;
    void TriggerTeardown() noexcept override;
    void TriggerSelection() override;
    void TriggerScroll() override;
    void TriggerScroll(const COORD* const pcoordDelta) override;
    void TriggerCircling() override;
    void TriggerTitleChange() override;

private:
    SCREEN_INFORMATION& _owner;
};
