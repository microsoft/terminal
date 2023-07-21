/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- XtermEngine.hpp

Abstract:
- This is the definition of the VT specific implementation of the renderer.
    This is the xterm implementation, which supports advanced sequences such as
    inserting and deleting lines, but only 16 colors.

    This engine supports both xterm and xterm-ascii VT modes.
    The difference being that xterm-ascii will render any characters above 0x7f
        as '?', in order to support older legacy tools.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#pragma once

#include "vtrenderer.hpp"

namespace Microsoft::Console::Render
{
    class XtermEngine : public VtEngine
    {
    public:
        XtermEngine(_In_ wil::unique_hfile hPipe,
                    const Microsoft::Console::Types::Viewport initialViewport,
                    const bool fUseAsciiOnly);

        virtual ~XtermEngine() override = default;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                           const RenderSettings& renderSettings,
                                                           const gsl::not_null<IRenderData*> pData,
                                                           const bool usingSoftFont,
                                                           const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters,
                                              const til::point coord,
                                              const bool trimLeft,
                                              const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT InvalidateScroll(const til::point* const pcoordDelta) noexcept override;

        [[nodiscard]] HRESULT WriteTerminalW(const std::wstring_view str) noexcept override;

        [[nodiscard]] HRESULT SetWindowVisibility(const bool showOrHide) noexcept override;

    protected:
        // I'm using a non-class enum here, so that the values
        // are trivially convertible and comparable to bool.
        enum class Tribool : uint8_t
        {
            False = 0,
            True,
            Invalid,
        };

        const bool _fUseAsciiOnly;
        bool _needToDisableCursor;
        Tribool _lastCursorIsVisible;
        bool _nextCursorIsVisible;

        [[nodiscard]] HRESULT _MoveCursor(const til::point coord) noexcept override;

        [[nodiscard]] HRESULT _DoUpdateTitle(const std::wstring_view newTitle) noexcept override;

#ifdef UNIT_TESTING
        friend class VtRendererTest;
        friend class ConptyOutputTests;
#endif
    };
}
