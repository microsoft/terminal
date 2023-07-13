/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VtRenderer.hpp

Abstract:
- This is the definition of the VT specific implementation of the renderer.

Author(s):
- Michael Niksa (MiNiksa) 24-Jul-2017
- Mike Griese (migrie) 01-Sept-2017
--*/

#pragma once

#include "../inc/RenderEngineBase.hpp"
#include "../../types/inc/Viewport.hpp"
#include "tracing.hpp"
#include <string>
#include <functional>

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalCoreUnitTests
{
    class ConptyRoundtripTests;
};
class ScreenBufferTests;
#endif

namespace Microsoft::Console::VirtualTerminal
{
    class VtIo;
}

namespace Microsoft::Console::Render
{
    class VtEngine : public RenderEngineBase
    {
    public:
        // See _PaintUtf8BufferLine for explanation of this value.
        static const size_t ERASE_CHARACTER_STRING_LENGTH = 8;
        static const til::point INVALID_COORDS;

        VtEngine(_In_ wil::unique_hfile hPipe,
                 const Microsoft::Console::Types::Viewport initialViewport);

        // IRenderEngine
        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* pForcePaint) noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateFlush(_In_ const bool circled, _Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(const LineRendition lineRendition, const til::CoordType targetRow, const til::CoordType viewportLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF color, size_t cchLine, til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(std::wstring_view glyph, _Out_ bool* pResult) noexcept override;

        // VtEngine
        [[nodiscard]] HRESULT SuppressResizeRepaint() noexcept;
        [[nodiscard]] HRESULT RequestCursor() noexcept;
        [[nodiscard]] HRESULT InheritCursor(const til::point coordCursor) noexcept;
        [[nodiscard]] HRESULT WriteTerminalUtf8(const std::string_view str) noexcept;
        [[nodiscard]] virtual HRESULT WriteTerminalW(const std::wstring_view str) noexcept = 0;
        void SetTerminalOwner(Microsoft::Console::VirtualTerminal::VtIo* const terminalOwner);
        void BeginResizeRequest();
        void EndResizeRequest();
        void SetResizeQuirk(const bool resizeQuirk);
        void SetPassthroughMode(const bool passthrough) noexcept;
        void SetLookingForDSRCallback(std::function<void(bool)> pfnLooking) noexcept;
        void SetTerminalCursorTextPosition(const til::point coordCursor) noexcept;
        [[nodiscard]] virtual HRESULT ManuallyClearScrollback() noexcept;
        [[nodiscard]] HRESULT RequestWin32Input() noexcept;
        [[nodiscard]] virtual HRESULT SetWindowVisibility(const bool showOrHide) noexcept = 0;
        [[nodiscard]] HRESULT SwitchScreenBuffer(const bool useAltBuffer) noexcept;

    protected:
        wil::unique_hfile _hFile;
        std::string _buffer;

        std::string _formatBuffer;
        std::string _conversionBuffer;

        bool _usingLineRenditions;
        bool _stopUsingLineRenditions;
        bool _usingSoftFont;
        TextAttribute _lastTextAttributes;

        std::function<void(bool)> _pfnSetLookingForDSR;

        Microsoft::Console::Types::Viewport _lastViewport;

        std::pmr::unsynchronized_pool_resource _pool;
        til::pmr::bitmap _invalidMap;

        til::point _lastText;
        til::point _scrollDelta;

        bool _quickReturn;
        bool _clearedAllThisFrame;
        bool _cursorMoved;
        bool _resized;

        bool _suppressResizeRepaint;

        til::CoordType _virtualTop;
        bool _circled;
        bool _firstPaint;
        bool _skipCursor;
        bool _newBottomLine;
        til::point _deferredCursorPos;

        HRESULT _exitResult;
        Microsoft::Console::VirtualTerminal::VtIo* _terminalOwner;

        Microsoft::Console::VirtualTerminal::RenderTracing _trace;
        bool _inResizeRequest{ false };

        std::optional<til::CoordType> _wrappedRow{ std::nullopt };

        bool _delayedEolWrap{ false };

        bool _resizeQuirk{ false };
        bool _passthrough{ false };
        bool _noFlushOnEnd{ false };
        std::optional<TextColor> _newBottomLineBG{ std::nullopt };

        [[nodiscard]] HRESULT _WriteFill(const size_t n, const char c) noexcept;
        [[nodiscard]] HRESULT _Write(std::string_view const str) noexcept;
        [[nodiscard]] HRESULT _Flush() noexcept;

        template<typename S, typename... Args>
        [[nodiscard]] HRESULT _WriteFormatted(S&& format, Args&&... args)
        try
        {
            fmt::basic_memory_buffer<char, 64> buf;
            fmt::format_to(std::back_inserter(buf), std::forward<S>(format), std::forward<Args>(args)...);
            return _Write({ buf.data(), buf.size() });
        }
        CATCH_RETURN()

        void _OrRect(_Inout_ til::inclusive_rect* const pRectExisting, const til::inclusive_rect* const pRectToOr) const;
        bool _AllIsInvalid() const;

        [[nodiscard]] HRESULT _StopCursorBlinking() noexcept;
        [[nodiscard]] HRESULT _StartCursorBlinking() noexcept;
        [[nodiscard]] HRESULT _HideCursor() noexcept;
        [[nodiscard]] HRESULT _ShowCursor() noexcept;
        [[nodiscard]] HRESULT _EraseLine() noexcept;
        [[nodiscard]] HRESULT _InsertDeleteLine(const til::CoordType sLines, const bool fInsertLine) noexcept;
        [[nodiscard]] HRESULT _DeleteLine(const til::CoordType sLines) noexcept;
        [[nodiscard]] HRESULT _InsertLine(const til::CoordType sLines) noexcept;
        [[nodiscard]] HRESULT _CursorForward(const til::CoordType chars) noexcept;
        [[nodiscard]] HRESULT _EraseCharacter(const til::CoordType chars) noexcept;
        [[nodiscard]] HRESULT _CursorPosition(const til::point coord) noexcept;
        [[nodiscard]] HRESULT _CursorHome() noexcept;
        [[nodiscard]] HRESULT _ClearScreen() noexcept;
        [[nodiscard]] HRESULT _ClearScrollback() noexcept;
        [[nodiscard]] HRESULT _ChangeTitle(const std::string& title) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRendition16Color(const BYTE index,
                                                           const bool fIsForeground) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRendition256Color(const BYTE index,
                                                            const bool fIsForeground) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRenditionRGBColor(const COLORREF color,
                                                            const bool fIsForeground) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRenditionDefaultColor(const bool fIsForeground) noexcept;

        [[nodiscard]] HRESULT _SetGraphicsDefault() noexcept;

        [[nodiscard]] HRESULT _ResizeWindow(const til::CoordType sWidth, const til::CoordType sHeight) noexcept;

        [[nodiscard]] HRESULT _SetIntense(const bool isIntense) noexcept;
        [[nodiscard]] HRESULT _SetFaint(const bool isFaint) noexcept;
        [[nodiscard]] HRESULT _SetUnderlined(const bool isUnderlined) noexcept;
        [[nodiscard]] HRESULT _SetDoublyUnderlined(const bool isUnderlined) noexcept;
        [[nodiscard]] HRESULT _SetOverlined(const bool isOverlined) noexcept;
        [[nodiscard]] HRESULT _SetItalic(const bool isItalic) noexcept;
        [[nodiscard]] HRESULT _SetBlinking(const bool isBlinking) noexcept;
        [[nodiscard]] HRESULT _SetInvisible(const bool isInvisible) noexcept;
        [[nodiscard]] HRESULT _SetCrossedOut(const bool isCrossedOut) noexcept;
        [[nodiscard]] HRESULT _SetReverseVideo(const bool isReversed) noexcept;

        [[nodiscard]] HRESULT _SetHyperlink(const std::wstring_view& uri, const std::wstring_view& customId, const uint16_t& numberId) noexcept;
        [[nodiscard]] HRESULT _EndHyperlink() noexcept;

        [[nodiscard]] HRESULT _RequestCursor() noexcept;
        [[nodiscard]] HRESULT _ListenForDSR() noexcept;

        [[nodiscard]] HRESULT _RequestWin32Input() noexcept;
        [[nodiscard]] HRESULT _SwitchScreenBuffer(const bool useAltBuffer) noexcept;

        [[nodiscard]] HRESULT _RequestFocusEventMode() noexcept;

        [[nodiscard]] virtual HRESULT _MoveCursor(const til::point coord) noexcept = 0;
        [[nodiscard]] HRESULT _RgbUpdateDrawingBrushes(const TextAttribute& textAttributes) noexcept;
        [[nodiscard]] HRESULT _16ColorUpdateDrawingBrushes(const TextAttribute& textAttributes) noexcept;

        bool _WillWriteSingleChar() const;

        // buffer space for these two functions to build their lines
        // so they don't have to alloc/free in a tight loop
        std::wstring _bufferLine;
        [[nodiscard]] HRESULT _PaintUtf8BufferLine(const std::span<const Cluster> clusters,
                                                   const til::point coord,
                                                   const bool lineWrapped) noexcept;

        [[nodiscard]] HRESULT _PaintAsciiBufferLine(const std::span<const Cluster> clusters,
                                                    const til::point coord) noexcept;

        [[nodiscard]] HRESULT _WriteTerminalUtf8(const std::wstring_view str) noexcept;
        [[nodiscard]] HRESULT _WriteTerminalAscii(const std::wstring_view str) noexcept;
        [[nodiscard]] HRESULT _WriteTerminalDrcs(const std::wstring_view str) noexcept;

        [[nodiscard]] HRESULT _DoUpdateTitle(const std::wstring_view newTitle) noexcept override;

        /////////////////////////// Unit Testing Helpers ///////////////////////////
#ifdef UNIT_TESTING
        std::function<bool(const char* const, size_t const)> _pfnTestCallback;
        bool _usingTestCallback;

        friend class VtRendererTest;
        friend class ConptyOutputTests;
        friend class ScreenBufferTests;
        friend class TerminalCoreUnitTests::ConptyRoundtripTests;
#endif

        void SetTestCallback(_In_ std::function<bool(const char* const, size_t const)> pfn);
    };
}
