/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WinTelnetEngine.hpp

Abstract:
- This is the definition of the VT specific implementation of the renderer.
    This is the win-telnet implementation, which does NOT support advanced
    sequences such as inserting and deleting lines, and only supports 16 colors.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#pragma once

#include "vtrenderer.hpp"

namespace Microsoft::Console::Render
{
    class WinTelnetEngine : public VtEngine
    {
    public:
        WinTelnetEngine(_In_ wil::unique_hfile hPipe,
                        const Microsoft::Console::IDefaultColorProvider& colorProvider,
                        const Microsoft::Console::Types::Viewport initialViewport,
                        _In_reads_(cColorTable) const COLORREF* const ColorTable,
                        const WORD cColorTable);
        virtual ~WinTelnetEngine() override = default;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                   const COLORREF colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;

        [[nodiscard]] HRESULT WriteTerminalW(const std::wstring& wstr) noexcept override;

    protected:
        [[nodiscard]] HRESULT _MoveCursor(const COORD coord) noexcept;

    private:
        const COLORREF* const _ColorTable;
        const WORD _cColorTable;

#ifdef UNIT_TESTING
        friend class VtRendererTest;
#endif
    };
}
