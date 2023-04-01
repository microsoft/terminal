/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Xterm256Engine.hpp

Abstract:
- This is the definition of the VT specific implementation of the renderer.
    This is the xterm-256color implementation, which supports advanced sequences such as
    inserting and deleting lines, and true rgb color.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#pragma once

#include "XtermEngine.hpp"

class VtApiRoutines;

namespace Microsoft::Console::Render
{
    class Xterm256Engine : public XtermEngine
    {
    public:
        Xterm256Engine(_In_ wil::unique_hfile hPipe,
                       const Microsoft::Console::Types::Viewport initialViewport);

        virtual ~Xterm256Engine() override = default;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const RenderSettings& renderSettings,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool usingSoftFont,
                                                   const bool isSettingDefaultBrushes) noexcept override;

        [[nodiscard]] HRESULT ManuallyClearScrollback() noexcept override;

        friend class ::VtApiRoutines;

    private:
        [[nodiscard]] HRESULT _UpdateExtendedAttrs(const TextAttribute& textAttributes) noexcept;
        [[nodiscard]] HRESULT _UpdateHyperlinkAttr(const TextAttribute& textAttributes,
                                                   const gsl::not_null<IRenderData*> pData) noexcept;

#ifdef UNIT_TESTING
        friend class VtRendererTest;
        friend class ConptyOutputTests;
#endif
    };
}
