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

namespace Microsoft::Console::Render
{
    class Xterm256Engine : public XtermEngine
    {
    public:
        Xterm256Engine(_In_ wil::unique_hfile hPipe,
                       const Microsoft::Console::IDefaultColorProvider& colorProvider,
                       const Microsoft::Console::Types::Viewport initialViewport,
                       _In_reads_(cColorTable) const COLORREF* const ColorTable,
                       const WORD cColorTable);

        virtual ~Xterm256Engine() override = default;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                   const COLORREF colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   const bool isSettingDefaultBrushes) noexcept override;

    private:
        [[nodiscard]] HRESULT _UpdateExtendedAttrs(const ExtendedAttributes extendedAttrs) noexcept;

        // We're only using Italics, Blinking, Invisible and Crossed Out for now
        // See GH#2916 for adding a more complete implementation.
        ExtendedAttributes _lastExtendedAttrsState;

#ifdef UNIT_TESTING
        friend class VtRendererTest;
#endif
    };
}
