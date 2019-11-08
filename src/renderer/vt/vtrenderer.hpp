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
#include "../../inc/IDefaultColorProvider.hpp"
#include "../../inc/ITerminalOutputConnection.hpp"
#include "../../inc/ITerminalOwner.hpp"
#include "../../types/inc/Viewport.hpp"
#include "tracing.hpp"
#include <string>
#include <functional>

namespace Microsoft::Console::Render
{
    class VtEngine : public RenderEngineBase, public Microsoft::Console::ITerminalOutputConnection
    {
    public:
        // See _PaintUtf8BufferLine for explanation of this value.
        static const size_t ERASE_CHARACTER_STRING_LENGTH = 8;
        static const COORD INVALID_COORDS;

        VtEngine(_In_ wil::unique_hfile hPipe,
                 const Microsoft::Console::IDefaultColorProvider& colorProvider,
                 const Microsoft::Console::Types::Viewport initialViewport);

        virtual ~VtEngine() override = default;

        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] virtual HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept = 0;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] virtual HRESULT StartPaint() noexcept override;
        [[nodiscard]] virtual HRESULT EndPaint() noexcept override;
        [[nodiscard]] virtual HRESULT Present() noexcept override;

        [[nodiscard]] virtual HRESULT ScrollFrame() noexcept = 0;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] virtual HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                      const COORD coord,
                                                      const bool trimLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLines lines,
                                                   const COLORREF color,
                                                   const size_t cchLine,
                                                   const COORD coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] virtual HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const WORD legacyColorAttribute,
                                                           const ExtendedAttributes extendedAttrs,
                                                           const bool isSettingDefaultBrushes) noexcept = 0;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& pfiFontInfoDesired,
                                         _Out_ FontInfo& pfiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontDesired,
                                              _Out_ FontInfo& Font,
                                              const int iDpi) noexcept override;

        SMALL_RECT GetDirtyRectInChars() override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

        [[nodiscard]] HRESULT SuppressResizeRepaint() noexcept;

        [[nodiscard]] HRESULT RequestCursor() noexcept;
        [[nodiscard]] HRESULT InheritCursor(const COORD coordCursor) noexcept;

        [[nodiscard]] HRESULT WriteTerminalUtf8(const std::string& str) noexcept;

        [[nodiscard]] virtual HRESULT WriteTerminalW(const std::wstring& str) noexcept = 0;

        void SetTerminalOwner(Microsoft::Console::ITerminalOwner* const terminalOwner);
        void BeginResizeRequest();
        void EndResizeRequest();

    protected:
        wil::unique_hfile _hFile;
        std::string _buffer;

        const Microsoft::Console::IDefaultColorProvider& _colorProvider;

        COLORREF _LastFG;
        COLORREF _LastBG;
        bool _lastWasBold;

        Microsoft::Console::Types::Viewport _lastViewport;
        Microsoft::Console::Types::Viewport _invalidRect;

        bool _fInvalidRectUsed;
        COORD _lastRealCursor;
        COORD _lastText;
        COORD _scrollDelta;

        bool _quickReturn;
        bool _clearedAllThisFrame;
        bool _cursorMoved;
        bool _resized;

        bool _suppressResizeRepaint;

        SHORT _virtualTop;
        bool _circled;
        bool _firstPaint;
        bool _skipCursor;
        bool _newBottomLine;
        COORD _deferredCursorPos;

        bool _pipeBroken;
        HRESULT _exitResult;
        Microsoft::Console::ITerminalOwner* _terminalOwner;

        Microsoft::Console::VirtualTerminal::RenderTracing _trace;
        bool _inResizeRequest{ false };

        [[nodiscard]] HRESULT _Write(std::string_view const str) noexcept;
        [[nodiscard]] HRESULT _WriteFormattedString(const std::string* const pFormat, ...) noexcept;
        [[nodiscard]] HRESULT _Flush() noexcept;

        void _OrRect(_Inout_ SMALL_RECT* const pRectExisting, const SMALL_RECT* const pRectToOr) const;
        [[nodiscard]] HRESULT _InvalidCombine(const Microsoft::Console::Types::Viewport invalid) noexcept;
        [[nodiscard]] HRESULT _InvalidOffset(const COORD* const ppt) noexcept;
        [[nodiscard]] HRESULT _InvalidRestrict() noexcept;
        bool _AllIsInvalid() const;

        [[nodiscard]] HRESULT _StopCursorBlinking() noexcept;
        [[nodiscard]] HRESULT _StartCursorBlinking() noexcept;
        [[nodiscard]] HRESULT _HideCursor() noexcept;
        [[nodiscard]] HRESULT _ShowCursor() noexcept;
        [[nodiscard]] HRESULT _EraseLine() noexcept;
        [[nodiscard]] HRESULT _InsertDeleteLine(const short sLines, const bool fInsertLine) noexcept;
        [[nodiscard]] HRESULT _DeleteLine(const short sLines) noexcept;
        [[nodiscard]] HRESULT _InsertLine(const short sLines) noexcept;
        [[nodiscard]] HRESULT _CursorForward(const short chars) noexcept;
        [[nodiscard]] HRESULT _EraseCharacter(const short chars) noexcept;
        [[nodiscard]] HRESULT _CursorPosition(const COORD coord) noexcept;
        [[nodiscard]] HRESULT _CursorHome() noexcept;
        [[nodiscard]] HRESULT _ClearScreen() noexcept;
        [[nodiscard]] HRESULT _ChangeTitle(const std::string& title) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRendition16Color(const WORD wAttr,
                                                           const bool fIsForeground) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRenditionRGBColor(const COLORREF color,
                                                            const bool fIsForeground) noexcept;
        [[nodiscard]] HRESULT _SetGraphicsRenditionDefaultColor(const bool fIsForeground) noexcept;

        [[nodiscard]] HRESULT _SetGraphicsBoldness(const bool isBold) noexcept;

        [[nodiscard]] HRESULT _SetGraphicsDefault() noexcept;

        [[nodiscard]] HRESULT _ResizeWindow(const short sWidth, const short sHeight) noexcept;

        [[nodiscard]] HRESULT _BeginUnderline() noexcept;
        [[nodiscard]] HRESULT _EndUnderline() noexcept;

        [[nodiscard]] HRESULT _BeginItalics() noexcept;
        [[nodiscard]] HRESULT _EndItalics() noexcept;

        [[nodiscard]] HRESULT _BeginBlink() noexcept;
        [[nodiscard]] HRESULT _EndBlink() noexcept;

        [[nodiscard]] HRESULT _BeginInvisible() noexcept;
        [[nodiscard]] HRESULT _EndInvisible() noexcept;

        [[nodiscard]] HRESULT _BeginCrossedOut() noexcept;
        [[nodiscard]] HRESULT _EndCrossedOut() noexcept;

        [[nodiscard]] HRESULT _RequestCursor() noexcept;

        [[nodiscard]] virtual HRESULT _MoveCursor(const COORD coord) noexcept = 0;
        [[nodiscard]] HRESULT _RgbUpdateDrawingBrushes(const COLORREF colorForeground,
                                                       const COLORREF colorBackground,
                                                       const bool isBold,
                                                       _In_reads_(cColorTable) const COLORREF* const ColorTable,
                                                       const WORD cColorTable) noexcept;
        [[nodiscard]] HRESULT _16ColorUpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const bool isBold,
                                                           _In_reads_(cColorTable) const COLORREF* const ColorTable,
                                                           const WORD cColorTable) noexcept;

        bool _WillWriteSingleChar() const;

        [[nodiscard]] HRESULT _PaintUtf8BufferLine(std::basic_string_view<Cluster> const clusters,
                                                   const COORD coord) noexcept;

        [[nodiscard]] HRESULT _PaintAsciiBufferLine(std::basic_string_view<Cluster> const clusters,
                                                    const COORD coord) noexcept;

        [[nodiscard]] HRESULT _WriteTerminalUtf8(const std::wstring& str) noexcept;
        [[nodiscard]] HRESULT _WriteTerminalAscii(const std::wstring& str) noexcept;

        [[nodiscard]] virtual HRESULT _DoUpdateTitle(const std::wstring& newTitle) noexcept override;

        /////////////////////////// Unit Testing Helpers ///////////////////////////
#ifdef UNIT_TESTING
        std::function<bool(const char* const, size_t const)> _pfnTestCallback;
        bool _usingTestCallback;

        friend class VtRendererTest;
#endif

        void SetTestCallback(_In_ std::function<bool(const char* const, size_t const)> pfn);
    };
}
