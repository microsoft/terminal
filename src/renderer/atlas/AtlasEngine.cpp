// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include <shader_ps.h>
#include <shader_vs.h>

#include "../base/FontCache.h"
#include "../../interactivity/win32/CustomWindowMessages.h"

// #### NOTE ####
// This file should only contain methods that are only accessed by the caller of Present() (the "Renderer" class).
// Basically this file poses the "synchronization" point between the concurrently running
// general IRenderEngine API (like the Invalidate*() methods) and the Present() method
// and thus may access both _r and _api.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render;

#pragma warning(push)
#pragma warning(disable : 26447) // The function is declared 'noexcept' but calls function 'operator()()' which may throw exceptions (f.6).
__declspec(noinline) static void showOOMWarning() noexcept
{
    [[maybe_unused]] static const auto once = []() {
        std::thread t{ []() noexcept {
            MessageBoxW(nullptr, L"This application is using a highly experimental text rendering engine and has run out of memory. Text rendering will start to behave irrationally and you should restart this process.", L"Out Of Memory", MB_ICONERROR | MB_OK);
        } };
        t.detach();
        return false;
    }();
}
#pragma warning(pop)

struct TextAnalyzer final : IDWriteTextAnalysisSource, IDWriteTextAnalysisSink
{
    constexpr TextAnalyzer(const std::vector<wchar_t>& text, std::vector<AtlasEngine::TextAnalyzerResult>& results) noexcept :
        _text{ text }, _results{ results }
    {
        Ensures(_text.size() <= UINT32_MAX);
    }

    // TextAnalyzer will be allocated on the stack and reference counting is pointless because of that.
    // The debug version will assert that we don't leak any references though.
#ifdef NDEBUG
    ULONG __stdcall AddRef() noexcept override
    {
        return 1;
    }

    ULONG __stdcall Release() noexcept override
    {
        return 1;
    }
#else
    ULONG _refCount = 1;

    ~TextAnalyzer()
    {
        assert(_refCount == 1);
    }

    ULONG __stdcall AddRef() noexcept override
    {
        return ++_refCount;
    }

    ULONG __stdcall Release() noexcept override
    {
        return --_refCount;
    }
#endif

    HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        __assume(ppvObject != nullptr);

        if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource)) || IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink)))
        {
            *ppvObject = this;
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT __stdcall GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        // Writing to address 0 is a crash in practice. Just what we want.
        __assume(textString != nullptr);
        __assume(textLength != nullptr);

        const auto size = gsl::narrow_cast<UINT32>(_text.size());
        textPosition = std::min(textPosition, size);
        *textString = _text.data() + textPosition;
        *textLength = size - textPosition;
        return S_OK;
    }

    HRESULT __stdcall GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override
    {
        // Writing to address 0 is a crash in practice. Just what we want.
        __assume(textString != nullptr);
        __assume(textLength != nullptr);

        const auto size = gsl::narrow_cast<UINT32>(_text.size());
        textPosition = std::min(textPosition, size);
        *textString = _text.data();
        *textLength = textPosition;
        return S_OK;
    }

    DWRITE_READING_DIRECTION __stdcall GetParagraphReadingDirection() noexcept override
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }

    HRESULT __stdcall GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) noexcept override
    {
        // Writing to address 0 is a crash in practice. Just what we want.
        __assume(textLength != nullptr);
        __assume(localeName != nullptr);

        *textLength = gsl::narrow_cast<UINT32>(_text.size()) - textPosition;
        *localeName = nullptr;
        return S_OK;
    }

    HRESULT __stdcall GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) noexcept override
    try
    {
        _results.emplace_back(AtlasEngine::TextAnalyzerResult{ textPosition, textLength, scriptAnalysis->script, static_cast<UINT8>(scriptAnalysis->shapes), 0 });
        return S_OK;
    }
    CATCH_RETURN()

    HRESULT __stdcall SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT __stdcall SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) noexcept override
    {
        return E_NOTIMPL;
    }

private:
    const std::vector<wchar_t>& _text;
    std::vector<AtlasEngine::TextAnalyzerResult>& _results;
};

#pragma warning(suppress : 26455) // Default constructor may not throw. Declare it 'noexcept' (f.6).
AtlasEngine::AtlasEngine()
{
#ifdef NDEBUG
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _sr.d2dFactory.addressof()));
#else
    static constexpr D2D1_FACTORY_OPTIONS options{ D2D1_DEBUG_LEVEL_INFORMATION };
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, _sr.d2dFactory.addressof()));
#endif

    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(_sr.dwriteFactory), reinterpret_cast<::IUnknown**>(_sr.dwriteFactory.addressof())));
    if (const auto factory2 = _sr.dwriteFactory.try_query<IDWriteFactory2>())
    {
        THROW_IF_FAILED(factory2->GetSystemFontFallback(_sr.systemFontFallback.addressof()));
    }
    {
        wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
        THROW_IF_FAILED(_sr.dwriteFactory->CreateTextAnalyzer(textAnalyzer.addressof()));
        _sr.textAnalyzer = textAnalyzer.query<IDWriteTextAnalyzer1>();
    }

    _sr.isWindows10OrGreater = IsWindows10OrGreater();

#ifndef NDEBUG
    {
        _sr.sourceDirectory = std::filesystem::path{ __FILE__ }.parent_path();
        _sr.sourceCodeWatcher = wil::make_folder_change_reader_nothrow(_sr.sourceDirectory.c_str(), false, wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime, [this](wil::FolderChangeEvent, PCWSTR path) {
            if (til::ends_with(path, L".hlsl"))
            {
                auto expected = INT64_MAX;
                const auto invalidationTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
                _sr.sourceCodeInvalidationTime.compare_exchange_strong(expected, invalidationTime.time_since_epoch().count(), std::memory_order_relaxed);
            }
        });
    }
#endif
}

#pragma region IRenderEngine

// StartPaint() is called while the console buffer lock is being held.
// --> Put as little in here as possible.
[[nodiscard]] HRESULT AtlasEngine::StartPaint() noexcept
try
{
    if (_api.hwnd)
    {
        RECT rect;
        LOG_IF_WIN32_BOOL_FALSE(GetClientRect(_api.hwnd, &rect));
        std::ignore = SetWindowSize({ rect.right - rect.left, rect.bottom - rect.top });

        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Title))
        {
            LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_api.hwnd, CM_UPDATE_TITLE, 0, 0));
            WI_ClearFlag(_api.invalidations, ApiInvalidations::Title);
        }
    }

    // It's important that we invalidate here instead of in Present() with the rest.
    // Other functions, those called before Present(), might depend on _r fields.
    // But most of the time _invalidations will be ::none, making this very cheap.
    if (_api.invalidations != ApiInvalidations::None)
    {
        RETURN_HR_IF(E_UNEXPECTED, _api.cellCount == u16x2{});

        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Device))
        {
            _createResources();
        }
        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::SwapChain))
        {
            _createSwapChain();
        }
        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Size))
        {
            _recreateSizeDependentResources();
        }
        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Font))
        {
            _recreateFontDependentResources();
        }
        if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Settings))
        {
            _r.selectionColor = _api.selectionColor;
            WI_SetFlag(_r.invalidations, RenderInvalidations::ConstBuffer);
        }

        // Equivalent to InvalidateAll().
        _api.invalidatedRows = invalidatedRowsAll;
    }

#ifndef NDEBUG
    if (const auto invalidationTime = _sr.sourceCodeInvalidationTime.load(std::memory_order_relaxed); invalidationTime != INT64_MAX && invalidationTime <= std::chrono::steady_clock::now().time_since_epoch().count())
    {
        _sr.sourceCodeInvalidationTime.store(INT64_MAX, std::memory_order_relaxed);

        try
        {
            static const auto compile = [](const std::filesystem::path& path, const char* target) {
                wil::com_ptr<ID3DBlob> error;
                wil::com_ptr<ID3DBlob> blob;
                const auto hr = D3DCompileFromFile(
                    /* pFileName   */ path.c_str(),
                    /* pDefines    */ nullptr,
                    /* pInclude    */ D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    /* pEntrypoint */ "main",
                    /* pTarget     */ target,
                    /* Flags1      */ D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                    /* Flags2      */ 0,
                    /* ppCode      */ blob.addressof(),
                    /* ppErrorMsgs */ error.addressof());

                if (error)
                {
                    std::thread t{ [error = std::move(error)]() noexcept {
                        MessageBoxA(nullptr, static_cast<const char*>(error->GetBufferPointer()), "Compilation error", MB_ICONERROR | MB_OK);
                    } };
                    t.detach();
                }

                THROW_IF_FAILED(hr);
                return blob;
            };

            const auto vs = compile(_sr.sourceDirectory / L"shader_vs.hlsl", "vs_4_1");
            const auto ps = compile(_sr.sourceDirectory / L"shader_ps.hlsl", "ps_4_1");

            THROW_IF_FAILED(_r.device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, _r.vertexShader.put()));
            THROW_IF_FAILED(_r.device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, _r.pixelShader.put()));
            _setShaderResources();
        }
        CATCH_LOG()
    }
#endif

    if (_api.invalidatedRows == invalidatedRowsAll)
    {
        // Skip all the partial updates, since we redraw everything anyways.
        _api.invalidatedCursorArea = invalidatedAreaNone;
        _api.invalidatedRows = { 0, _api.cellCount.y };
        _api.scrollOffset = 0;
    }
    else
    {
        // Clamp invalidation rects into valid value ranges.
        {
            _api.invalidatedCursorArea.left = std::min(_api.invalidatedCursorArea.left, _api.cellCount.x);
            _api.invalidatedCursorArea.top = std::min(_api.invalidatedCursorArea.top, _api.cellCount.y);
            _api.invalidatedCursorArea.right = clamp(_api.invalidatedCursorArea.right, _api.invalidatedCursorArea.left, _api.cellCount.x);
            _api.invalidatedCursorArea.bottom = clamp(_api.invalidatedCursorArea.bottom, _api.invalidatedCursorArea.top, _api.cellCount.y);
        }
        {
            _api.invalidatedRows.x = std::min(_api.invalidatedRows.x, _api.cellCount.y);
            _api.invalidatedRows.y = clamp(_api.invalidatedRows.y, _api.invalidatedRows.x, _api.cellCount.y);
        }
        {
            const auto limit = gsl::narrow_cast<i16>(_api.cellCount.y & 0x7fff);
            _api.scrollOffset = gsl::narrow_cast<i16>(clamp<int>(_api.scrollOffset, -limit, limit));
        }

        // Scroll the buffer by the given offset and mark the newly uncovered rows as "invalid".
        if (_api.scrollOffset != 0)
        {
            const auto nothingInvalid = _api.invalidatedRows.x == _api.invalidatedRows.y;
            const auto offset = static_cast<ptrdiff_t>(_api.scrollOffset) * _api.cellCount.x;
            const auto data = _r.cells.data();
            auto count = _r.cells.size();
#pragma warning(suppress : 26494) // Variable 'dst' is uninitialized. Always initialize an object (type.5).
            Cell* dst;
#pragma warning(suppress : 26494) // Variable 'src' is uninitialized. Always initialize an object (type.5).
            Cell* src;

            if (_api.scrollOffset < 0)
            {
                // Scroll up (for instance when new text is being written at the end of the buffer).
                dst = data;
                src = data - offset;
                count += offset;

                const u16 endRow = _api.cellCount.y + _api.scrollOffset;
                _api.invalidatedRows.x = nothingInvalid ? endRow : std::min<u16>(_api.invalidatedRows.x, endRow);
                _api.invalidatedRows.y = _api.cellCount.y;
            }
            else
            {
                // Scroll down.
                dst = data + offset;
                src = data;
                count -= offset;

                _api.invalidatedRows.x = 0;
                _api.invalidatedRows.y = nothingInvalid ? _api.scrollOffset : std::max<u16>(_api.invalidatedRows.y, _api.scrollOffset);
            }

            memmove(dst, src, count * sizeof(Cell));
        }
    }

    _api.dirtyRect = til::rect{
        0,
        _api.invalidatedRows.x,
        _api.cellCount.x,
        _api.invalidatedRows.y,
    };

    return S_OK;
}
catch (const wil::ResultException& exception)
{
    return _handleException(exception);
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::EndPaint() noexcept
try
{
    _flushBufferLine();

    _api.invalidatedCursorArea = invalidatedAreaNone;
    _api.invalidatedRows = invalidatedRowsNone;
    _api.scrollOffset = 0;
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] bool AtlasEngine::RequiresContinuousRedraw() noexcept
{
    return continuousRedraw;
}

void AtlasEngine::WaitUntilCanRender() noexcept
{
    if constexpr (!debugGeneralPerformance)
    {
        if (_r.frameLatencyWaitableObject)
        {
            WaitForSingleObjectEx(_r.frameLatencyWaitableObject.get(), 100, true);
#ifndef NDEBUG
            _r.frameLatencyWaitableObjectUsed = true;
#endif
        }
        else
        {
            Sleep(8);
        }
    }
}

[[nodiscard]] HRESULT AtlasEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);
    *pForcePaint = false;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::ScrollFrame() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PrepareRenderInfo(const RenderFrameInfo& info) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::ResetLineTransform() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PrepareLineTransform(const LineRendition lineRendition, const size_t targetRow, const size_t viewportLeft) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PaintBackground() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::PaintBufferLine(const gsl::span<const Cluster> clusters, const COORD coord, const bool fTrimLeft, const bool lineWrapped) noexcept
try
{
    const auto x = gsl::narrow_cast<u16>(clamp<int>(coord.X, 0, _api.cellCount.x));
    const auto y = gsl::narrow_cast<u16>(clamp<int>(coord.Y, 0, _api.cellCount.y));

    if (_api.lastPaintBufferLineCoord.y != y)
    {
        _flushBufferLine();
    }

    _api.lastPaintBufferLineCoord = { x, y };
    _api.bufferLineWasHyperlinked = false;

    // Due to the current IRenderEngine interface (that wasn't refactored yet) we need to assemble
    // the current buffer line first as the remaining function operates on whole lines of text.
    {
        if (!_api.bufferLineColumn.empty())
        {
            _api.bufferLineColumn.pop_back();
        }

        auto column = x;
        for (const auto& cluster : clusters)
        {
            for (const auto& ch : cluster.GetText())
            {
                _api.bufferLine.emplace_back(ch);
                _api.bufferLineColumn.emplace_back(column);
            }

            column += gsl::narrow_cast<u16>(cluster.GetColumns());
        }

        _api.bufferLineColumn.emplace_back(column);

        const BufferLineMetadata metadata{ _api.currentColor, _api.flags };
        std::fill_n(_api.bufferLineMetadata.data() + x, column - x, metadata);
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintBufferGridLines(const GridLineSet lines, const COLORREF color, const size_t cchLine, const COORD coordTarget) noexcept
try
{
    if (!_api.bufferLineWasHyperlinked && lines.test(GridLines::Underline) && WI_IsFlagClear(_api.flags, CellFlags::Underline))
    {
        _api.bufferLineWasHyperlinked = true;

        WI_UpdateFlagsInMask(_api.flags, CellFlags::Underline | CellFlags::UnderlineDotted | CellFlags::UnderlineDouble, CellFlags::Underline);

        const BufferLineMetadata metadata{ _api.currentColor, _api.flags };
        const size_t x = _api.lastPaintBufferLineCoord.x;
        std::fill_n(_api.bufferLineMetadata.data() + x, _api.bufferLineMetadata.size() - x, metadata);
    }
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintSelection(SMALL_RECT rect) noexcept
try
{
    // Unfortunately there's no step after Renderer::_PaintBufferOutput that
    // would inform us that it's done with the last AtlasEngine::PaintBufferLine.
    // As such we got to call _flushBufferLine() here just to be sure.
    _flushBufferLine();
    _setCellFlags(rect, CellFlags::Selected, CellFlags::Selected);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::PaintCursor(const CursorOptions& options) noexcept
try
{
    // Unfortunately there's no step after Renderer::_PaintBufferOutput that
    // would inform us that it's done with the last AtlasEngine::PaintBufferLine.
    // As such we got to call _flushBufferLine() here just to be sure.
    _flushBufferLine();

    {
        const CachedCursorOptions cachedOptions{
            gsl::narrow_cast<u32>(options.fUseColor ? options.cursorColor | 0xff000000 : INVALID_COLOR),
            gsl::narrow_cast<u16>(options.cursorType),
            gsl::narrow_cast<u8>(options.ulCursorHeightPercent),
        };
        if (_r.cursorOptions != cachedOptions)
        {
            _r.cursorOptions = cachedOptions;
            WI_SetFlag(_r.invalidations, RenderInvalidations::Cursor);
        }
    }

    // Clear the previous cursor
    if (_api.invalidatedCursorArea.non_empty())
    {
        _setCellFlags(til::bit_cast<SMALL_RECT>(_api.invalidatedCursorArea), CellFlags::Cursor, CellFlags::None);
    }

    if (options.isOn)
    {
        const auto point = options.coordCursor;
        // TODO: options.coordCursor can contain invalid out of bounds coordinates when
        // the window is being resized and the cursor is on the last line of the viewport.
        const auto x = gsl::narrow_cast<SHORT>(std::min<int>(point.X, _r.cellCount.x - 1));
        const auto y = gsl::narrow_cast<SHORT>(std::min<int>(point.Y, _r.cellCount.y - 1));
        const SHORT right = x + 1 + (options.fIsDoubleWidth & (options.cursorType != CursorType::VerticalBar));
        const SHORT bottom = y + 1;
        _setCellFlags({ x, y, right, bottom }, CellFlags::Cursor, CellFlags::Cursor);
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, const gsl::not_null<IRenderData*> /*pData*/, const bool usingSoftFont, const bool isSettingDefaultBrushes) noexcept
try
{
    auto [fg, bg] = renderSettings.GetAttributeColorsWithAlpha(textAttributes);
    fg |= 0xff000000;
    bg |= _api.backgroundOpaqueMixin;

    if (!isSettingDefaultBrushes)
    {
        const auto hyperlinkId = textAttributes.GetHyperlinkId();

        auto flags = CellFlags::None;
        WI_SetFlagIf(flags, CellFlags::BorderLeft, textAttributes.IsLeftVerticalDisplayed());
        WI_SetFlagIf(flags, CellFlags::BorderTop, textAttributes.IsTopHorizontalDisplayed());
        WI_SetFlagIf(flags, CellFlags::BorderRight, textAttributes.IsRightVerticalDisplayed());
        WI_SetFlagIf(flags, CellFlags::BorderBottom, textAttributes.IsBottomHorizontalDisplayed());
        WI_SetFlagIf(flags, CellFlags::Underline, textAttributes.IsUnderlined());
        WI_SetFlagIf(flags, CellFlags::UnderlineDotted, hyperlinkId != 0);
        WI_SetFlagIf(flags, CellFlags::UnderlineDouble, textAttributes.IsDoublyUnderlined());
        WI_SetFlagIf(flags, CellFlags::Strikethrough, textAttributes.IsCrossedOut());

        if (_api.hyperlinkHoveredId && _api.hyperlinkHoveredId == hyperlinkId)
        {
            WI_SetFlag(flags, CellFlags::Underline);
            WI_ClearAllFlags(flags, CellFlags::UnderlineDotted | CellFlags::UnderlineDouble);
        }

        const u32x2 newColors{ gsl::narrow_cast<u32>(fg), gsl::narrow_cast<u32>(bg) };
        const AtlasKeyAttributes attributes{ 0, textAttributes.IsIntense(), textAttributes.IsItalic(), 0 };

        if (_api.attributes != attributes)
        {
            _flushBufferLine();
        }

        _api.currentColor = newColors;
        _api.attributes = attributes;
        _api.flags = flags;
    }
    else if (textAttributes.BackgroundIsDefault() && bg != _r.backgroundColor)
    {
        _r.backgroundColor = bg;
        WI_SetFlag(_r.invalidations, RenderInvalidations::ConstBuffer);
    }

    return S_OK;
}
CATCH_RETURN()

#pragma endregion

[[nodiscard]] HRESULT AtlasEngine::_handleException(const wil::ResultException& exception) noexcept
{
    const auto hr = exception.GetErrorCode();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == D2DERR_RECREATE_TARGET)
    {
        WI_SetFlag(_api.invalidations, ApiInvalidations::Device);
        return E_PENDING; // Indicate a retry to the renderer
    }

    // NOTE: This isn't thread safe, as _handleException is called by AtlasEngine.r.cpp.
    // However it's not too much of a concern at the moment as SetWarningCallback()
    // is only called once during construction in practice.
    if (_api.warningCallback)
    {
        try
        {
            _api.warningCallback(hr);
        }
        CATCH_LOG()
    }

    return hr;
}

void AtlasEngine::_createResources()
{
    _releaseSwapChain();
    _r = {};

#ifdef NDEBUG
    static constexpr
#endif
        auto deviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifndef NDEBUG
    // DXGI debug messages + enabling D3D11_CREATE_DEVICE_DEBUG if the Windows SDK was installed.
    if (const wil::unique_hmodule module{ LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) })
    {
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

        if (const auto DXGIGetDebugInterface1 = GetProcAddressByFunctionDeclaration(module.get(), DXGIGetDebugInterface1))
        {
            if (wil::com_ptr<IDXGIInfoQueue> infoQueue; SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(infoQueue.addressof()))))
            {
                // I didn't want to link with dxguid.lib just for getting DXGI_DEBUG_ALL. This GUID is publicly documented.
                static constexpr GUID dxgiDebugAll = { 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 } };
                for (const auto severity : std::array{ DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING })
                {
                    infoQueue->SetBreakOnSeverity(dxgiDebugAll, severity, true);
                }
            }

            if (wil::com_ptr<IDXGIDebug1> debug; SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(debug.addressof()))))
            {
                debug->EnableLeakTrackingForThread();
            }
        }
    }
#endif // NDEBUG

    // D3D device setup (basically a D3D class factory)
    {
        wil::com_ptr<ID3D11DeviceContext> deviceContext;

        // Why D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS:
        // This flag prevents the driver from creating a large thread pool for things like shader computations
        // that would be advantageous for games. For us this has only a minimal performance benefit,
        // but comes with a large memory usage overhead. At the time of writing the Nvidia
        // driver launches $cpu_thread_count more worker threads without this flag.
        static constexpr std::array driverTypes{
            std::pair{ D3D_DRIVER_TYPE_HARDWARE, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS },
            std::pair{ D3D_DRIVER_TYPE_WARP, static_cast<D3D11_CREATE_DEVICE_FLAG>(0) },
        };
        static constexpr std::array featureLevels{
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
        };

        auto hr = S_OK;
        for (const auto& [driverType, additionalFlags] : driverTypes)
        {
            hr = D3D11CreateDevice(
                /* pAdapter */ nullptr,
                /* DriverType */ driverType,
                /* Software */ nullptr,
                /* Flags */ deviceFlags | additionalFlags,
                /* pFeatureLevels */ featureLevels.data(),
                /* FeatureLevels */ gsl::narrow_cast<UINT>(featureLevels.size()),
                /* SDKVersion */ D3D11_SDK_VERSION,
                /* ppDevice */ _r.device.put(),
                /* pFeatureLevel */ nullptr,
                /* ppImmediateContext */ deviceContext.put());
            if (SUCCEEDED(hr))
            {
                break;
            }
        }
        THROW_IF_FAILED(hr);

        _r.deviceContext = deviceContext.query<ID3D11DeviceContext1>();
    }

#ifndef NDEBUG
    // D3D debug messages
    if (deviceFlags & D3D11_CREATE_DEVICE_DEBUG)
    {
        const auto infoQueue = _r.device.query<ID3D11InfoQueue>();
        for (const auto severity : std::array{ D3D11_MESSAGE_SEVERITY_CORRUPTION, D3D11_MESSAGE_SEVERITY_ERROR, D3D11_MESSAGE_SEVERITY_WARNING })
        {
            infoQueue->SetBreakOnSeverity(severity, true);
        }
    }
#endif // NDEBUG

    // Our constant buffer will never get resized
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(ConstBuffer);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        THROW_IF_FAILED(_r.device->CreateBuffer(&desc, nullptr, _r.constantBuffer.put()));
    }

    THROW_IF_FAILED(_r.device->CreateVertexShader(&shader_vs[0], sizeof(shader_vs), nullptr, _r.vertexShader.put()));
    THROW_IF_FAILED(_r.device->CreatePixelShader(&shader_ps[0], sizeof(shader_ps), nullptr, _r.pixelShader.put()));

    WI_ClearFlag(_api.invalidations, ApiInvalidations::Device);
    WI_SetAllFlags(_api.invalidations, ApiInvalidations::SwapChain);
}

void AtlasEngine::_releaseSwapChain()
{
    // Flush() docs:
    //   However, if an application must actually destroy an old swap chain and create a new swap chain,
    //   the application must force the destruction of all objects that the application freed.
    //   To force the destruction, call ID3D11DeviceContext::ClearState (or otherwise ensure
    //   no views are bound to pipeline state), and then call Flush on the immediate context.
    if (_r.swapChain && _r.deviceContext)
    {
        _r.frameLatencyWaitableObject.reset();
        _r.swapChain.reset();
        _r.renderTargetView.reset();
        _r.deviceContext->ClearState();
        _r.deviceContext->Flush();
    }
}

void AtlasEngine::_createSwapChain()
{
    _releaseSwapChain();

    // D3D swap chain setup (the thing that allows us to present frames on the screen)
    {
        const auto supportsFrameLatencyWaitableObject = IsWindows8Point1OrGreater();

        // With C++20 we'll finally have designated initializers.
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = _api.sizeInPixel.x;
        desc.Height = _api.sizeInPixel.y;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2; // TODO: 3?
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = _sr.isWindows10OrGreater ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        // * HWND swap chains can't do alpha.
        // * If our background is opaque we can enable "independent" flips by setting DXGI_SWAP_EFFECT_FLIP_DISCARD and DXGI_ALPHA_MODE_IGNORE.
        //   As our swap chain won't have to compose with DWM anymore it reduces the display latency dramatically.
        desc.AlphaMode = _api.hwnd || _api.backgroundOpaqueMixin ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;
        desc.Flags = supportsFrameLatencyWaitableObject ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;

        wil::com_ptr<IDXGIFactory2> dxgiFactory;
        THROW_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.addressof())));

        if (_api.hwnd)
        {
            if (FAILED(dxgiFactory->CreateSwapChainForHwnd(_r.device.get(), _api.hwnd, &desc, nullptr, nullptr, _r.swapChain.put())))
            {
                // Platform Update for Windows 7:
                // DXGI_SCALING_NONE is not supported on Windows 7 or Windows Server 2008 R2 with the Platform Update for
                // Windows 7 installed and causes CreateSwapChainForHwnd to return DXGI_ERROR_INVALID_CALL when called.
                desc.Scaling = DXGI_SCALING_STRETCH;
                THROW_IF_FAILED(dxgiFactory->CreateSwapChainForHwnd(_r.device.get(), _api.hwnd, &desc, nullptr, nullptr, _r.swapChain.put()));
            }
        }
        else
        {
            // We can't link with dcomp.lib as dcomp.dll doesn't exist on Windows 7.
            const wil::unique_hmodule module{ LoadLibraryExW(L"dcomp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) };
            THROW_LAST_ERROR_IF(!module);
            const auto DCompositionCreateSurfaceHandle = GetProcAddressByFunctionDeclaration(module.get(), DCompositionCreateSurfaceHandle);
            THROW_LAST_ERROR_IF(!DCompositionCreateSurfaceHandle);

            // As per: https://docs.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-dcompositioncreatesurfacehandle
            static constexpr DWORD COMPOSITIONSURFACE_ALL_ACCESS = 0x0003L;
            THROW_IF_FAILED(DCompositionCreateSurfaceHandle(COMPOSITIONSURFACE_ALL_ACCESS, nullptr, _api.swapChainHandle.put()));
            THROW_IF_FAILED(dxgiFactory.query<IDXGIFactoryMedia>()->CreateSwapChainForCompositionSurfaceHandle(_r.device.get(), _api.swapChainHandle.get(), &desc, nullptr, _r.swapChain.put()));
        }

        if (supportsFrameLatencyWaitableObject)
        {
            const auto swapChain2 = _r.swapChain.query<IDXGISwapChain2>();
            THROW_IF_FAILED(swapChain2->SetMaximumFrameLatency(1)); // TODO: 2?
            _r.frameLatencyWaitableObject.reset(swapChain2->GetFrameLatencyWaitableObject());
            THROW_LAST_ERROR_IF(!_r.frameLatencyWaitableObject);
        }
    }

    // See documentation for IDXGISwapChain2::GetFrameLatencyWaitableObject method:
    // > For every frame it renders, the app should wait on this handle before starting any rendering operations.
    // > Note that this requirement includes the first frame the app renders with the swap chain.
    WaitUntilCanRender();

    if (_api.swapChainChangedCallback)
    {
        try
        {
            _api.swapChainChangedCallback();
        }
        CATCH_LOG();
    }

    WI_ClearFlag(_api.invalidations, ApiInvalidations::SwapChain);
    WI_SetAllFlags(_api.invalidations, ApiInvalidations::Size | ApiInvalidations::Font);
}

void AtlasEngine::_recreateSizeDependentResources()
{
    // ResizeBuffer() docs:
    //   Before you call ResizeBuffers, ensure that the application releases all references [...].
    //   You can use ID3D11DeviceContext::ClearState to ensure that all [internal] references are released.
    if (_r.renderTargetView)
    {
        _r.renderTargetView.reset();
        _r.deviceContext->ClearState();
        _r.deviceContext->Flush();
        THROW_IF_FAILED(_r.swapChain->ResizeBuffers(0, _api.sizeInPixel.x, _api.sizeInPixel.y, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));
    }

    // The RenderTargetView is later used with OMSetRenderTargets
    // to tell D3D where stuff is supposed to be rendered at.
    {
        wil::com_ptr<ID3D11Texture2D> buffer;
        THROW_IF_FAILED(_r.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), buffer.put_void()));
        THROW_IF_FAILED(_r.device->CreateRenderTargetView(buffer.get(), nullptr, _r.renderTargetView.put()));
    }

    // Tell D3D which parts of the render target will be visible.
    // Everything outside of the viewport will be black.
    //
    // In the future this should cover the entire _api.sizeInPixel.x/_api.sizeInPixel.y.
    // The pixel shader should draw the remaining content in the configured background color.
    {
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(_api.sizeInPixel.x);
        viewport.Height = static_cast<float>(_api.sizeInPixel.y);
        _r.deviceContext->RSSetViewports(1, &viewport);
    }

    if (_api.cellCount != _r.cellCount)
    {
        const auto totalCellCount = static_cast<size_t>(_api.cellCount.x) * static_cast<size_t>(_api.cellCount.y);
        // Let's guess that every cell consists of a surrogate pair.
        const auto projectedTextSize = static_cast<size_t>(_api.cellCount.x) * 2;
        // IDWriteTextAnalyzer::GetGlyphs says:
        //   The recommended estimate for the per-glyph output buffers is (3 * textLength / 2 + 16).
        // We already set the textLength to twice the cell count.
        const auto projectedGlyphSize = 3 * projectedTextSize + 16;

        // This buffer is a bit larger than the others (multiple MB).
        // Prevent a memory usage spike, by first deallocating and then allocating.
        _r.cells = {};
        // Our render loop heavily relies on memcpy() which is between 1.5x
        // and 40x faster for allocations with an alignment of 32 or greater.
        // (40x on AMD Zen1-3, which have a rep movsb performance issue. MSFT:33358259.)
        _r.cells = Buffer<Cell, 32>{ totalCellCount };
        _r.cellCount = _api.cellCount;

        // .clear() doesn't free the memory of these buffers.
        // This code allows them to shrink again.
        _api.bufferLine = {};
        _api.bufferLine.reserve(projectedTextSize);
        _api.bufferLineColumn.reserve(projectedTextSize + 1);
        _api.bufferLineMetadata = Buffer<BufferLineMetadata>{ _api.cellCount.x };
        _api.analysisResults = {};

        _api.clusterMap = Buffer<u16>{ projectedTextSize };
        _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ projectedTextSize };
        _api.glyphIndices = Buffer<u16>{ projectedGlyphSize };
        _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>{ projectedGlyphSize };
        _api.glyphAdvances = Buffer<f32>{ projectedGlyphSize };
        _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ projectedGlyphSize };

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = gsl::narrow<u32>(totalCellCount * sizeof(Cell)); // totalCellCount can theoretically be UINT32_MAX!
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = sizeof(Cell);
        THROW_IF_FAILED(_r.device->CreateBuffer(&desc, nullptr, _r.cellBuffer.put()));
        THROW_IF_FAILED(_r.device->CreateShaderResourceView(_r.cellBuffer.get(), nullptr, _r.cellView.put()));
    }

    // We have called _r.deviceContext->ClearState() in the beginning and lost all D3D state.
    // This forces us to set up everything up from scratch again.
    _setShaderResources();

    WI_ClearFlag(_api.invalidations, ApiInvalidations::Size);
    WI_SetAllFlags(_r.invalidations, RenderInvalidations::ConstBuffer);
}

void AtlasEngine::_recreateFontDependentResources()
{
    {
        // We're likely resizing the atlas anyways and can
        // thus also release any of these buffers prematurely.
        _r.d2dRenderTarget.reset(); // depends on _r.atlasScratchpad
        _r.atlasScratchpad.reset();
        _r.atlasView.reset();
        _r.atlasBuffer.reset();
    }

    // D3D
    {
        // TODO: Consider using IDXGIAdapter3::QueryVideoMemoryInfo() and IDXGIAdapter3::RegisterVideoMemoryBudgetChangeNotificationEvent()
        // That way we can make better to use of a user's available video memory.

        static constexpr size_t sizePerPixel = 4;
        static constexpr size_t sizeLimit = D3D10_REQ_RESOURCE_SIZE_IN_MEGABYTES * 1024 * 1024;
        const size_t dimensionLimit = _r.device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ? D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION : D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        const size_t csx = _api.fontMetrics.cellSize.x;
        const size_t csy = _api.fontMetrics.cellSize.y;
        const auto xLimit = (dimensionLimit / csx) * csx;
        const auto pixelsPerCellRow = xLimit * csy;
        const auto yLimitDueToDimension = (dimensionLimit / csy) * csy;
        const auto yLimitDueToSize = ((sizeLimit / sizePerPixel) / pixelsPerCellRow) * csy;
        const auto yLimit = std::min(yLimitDueToDimension, yLimitDueToSize);
        const auto scaling = GetScaling();

        _r.cellSizeDIP.x = static_cast<float>(_api.fontMetrics.cellSize.x) / scaling;
        _r.cellSizeDIP.y = static_cast<float>(_api.fontMetrics.cellSize.y) / scaling;
        _r.cellSize = _api.fontMetrics.cellSize;
        _r.cellCount = _api.cellCount;
        // x/yLimit are strictly smaller than dimensionLimit, which is smaller than a u16.
        _r.atlasSizeInPixelLimit = u16x2{ gsl::narrow_cast<u16>(xLimit), gsl::narrow_cast<u16>(yLimit) };
        _r.atlasSizeInPixel = { 0, 0 };
        // The first Cell at {0, 0} is always our cursor texture.
        // --> The first glyph starts at {1, 0}.
        _r.atlasPosition.x = _api.fontMetrics.cellSize.x;
        _r.atlasPosition.y = 0;

        _r.glyphs = {};
        _r.glyphQueue = {};
        _r.glyphQueue.reserve(64);
    }
    // D3D specifically for UpdateDpi()
    // This compensates for the built in scaling factor in a XAML SwapChainPanel (CompositionScaleX/Y).
    if (!_api.hwnd)
    {
        if (const auto swapChain2 = _r.swapChain.try_query<IDXGISwapChain2>())
        {
            const auto inverseScale = static_cast<float>(USER_DEFAULT_SCREEN_DPI) / static_cast<float>(_api.dpi);
            DXGI_MATRIX_3X2_F matrix{};
            matrix._11 = inverseScale;
            matrix._22 = inverseScale;
            THROW_IF_FAILED(swapChain2->SetMatrixTransform(&matrix));
        }
    }

    // D2D
    {
        _r.underlinePos = _api.fontMetrics.underlinePos;
        _r.strikethroughPos = _api.fontMetrics.strikethroughPos;
        _r.lineThickness = _api.fontMetrics.lineThickness;
        _r.dpi = _api.dpi;
        _r.maxEncounteredCellCount = 0;
        _r.scratchpadCellWidth = 0;
    }
    {
        // See AtlasEngine::UpdateFont.
        // It hardcodes indices 0/1/2 in fontAxisValues to the weight/italic/slant axes.
        // If they're -1.0f they haven't been set by the user and must be filled by us.
        // When we call SetFontAxisValues() we basically override (disable) DirectWrite's internal font axes,
        // and if either of the 3 aren't set we'd make it impossible for the user to see bold/italic text.
#pragma warning(suppress : 26494) // Variable 'standardAxes' is uninitialized. Always initialize an object (type.5).
        std::array<DWRITE_FONT_AXIS_VALUE, 3> standardAxes;

        if (!_api.fontAxisValues.empty())
        {
            Expects(_api.fontAxisValues.size() >= standardAxes.size());
            memcpy(standardAxes.data(), _api.fontAxisValues.data(), sizeof(standardAxes));
        }

        const auto restoreFontAxisValues = wil::scope_exit([&]() noexcept {
            if (!_api.fontAxisValues.empty())
            {
                memcpy(_api.fontAxisValues.data(), standardAxes.data(), sizeof(standardAxes));
            }
        });

        for (auto italic = 0; italic < 2; ++italic)
        {
            for (auto bold = 0; bold < 2; ++bold)
            {
                const auto fontWeight = bold ? DWRITE_FONT_WEIGHT_BOLD : static_cast<DWRITE_FONT_WEIGHT>(_api.fontMetrics.fontWeight);
                const auto fontStyle = italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
                auto& textFormat = _r.textFormats[italic][bold];

                THROW_IF_FAILED(_sr.dwriteFactory->CreateTextFormat(_api.fontMetrics.fontName.get(), _api.fontMetrics.fontCollection.get(), fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL, _api.fontMetrics.fontSizeInDIP, L"", textFormat.put()));
                textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

                // DWRITE_LINE_SPACING_METHOD_UNIFORM:
                // > Lines are explicitly set to uniform spacing, regardless of contained font sizes.
                // > This can be useful to avoid the uneven appearance that can occur from font fallback.
                // We want that. Otherwise fallback fonts might be rendered with an incorrect baseline and get cut off vertically.
                THROW_IF_FAILED(textFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, _r.cellSizeDIP.y, _api.fontMetrics.baselineInDIP));

                if (!_api.fontAxisValues.empty())
                {
                    if (const auto textFormat3 = textFormat.try_query<IDWriteTextFormat3>())
                    {
                        // The wght axis defaults to the font weight.
                        _api.fontAxisValues[0].value = bold || standardAxes[0].value == -1.0f ? static_cast<float>(fontWeight) : standardAxes[0].value;
                        // The ital axis defaults to 1 if this is italic and 0 otherwise.
                        _api.fontAxisValues[1].value = italic ? 1.0f : (standardAxes[1].value == -1.0f ? 0.0f : standardAxes[1].value);
                        // The slnt axis defaults to -12 if this is italic and 0 otherwise.
                        _api.fontAxisValues[2].value = italic ? -12.0f : (standardAxes[2].value == -1.0f ? 0.0f : standardAxes[2].value);

                        THROW_IF_FAILED(textFormat3->SetFontAxisValues(_api.fontAxisValues.data(), gsl::narrow_cast<u32>(_api.fontAxisValues.size())));
                        _r.textFormatAxes[italic][bold] = { _api.fontAxisValues.data(), _api.fontAxisValues.size() };
                    }
                }
            }
        }
    }
    {
        _r.typography.reset();

        if (!_api.fontFeatures.empty())
        {
            _sr.dwriteFactory->CreateTypography(_r.typography.addressof());
            for (const auto& v : _api.fontFeatures)
            {
                THROW_IF_FAILED(_r.typography->AddFontFeature(v));
            }
        }
    }

    WI_ClearFlag(_api.invalidations, ApiInvalidations::Font);
    WI_SetAllFlags(_r.invalidations, RenderInvalidations::Cursor | RenderInvalidations::ConstBuffer);
}

IDWriteTextFormat* AtlasEngine::_getTextFormat(bool bold, bool italic) const noexcept
{
    return _r.textFormats[italic][bold].get();
}

const AtlasEngine::Buffer<DWRITE_FONT_AXIS_VALUE>& AtlasEngine::_getTextFormatAxis(bool bold, bool italic) const noexcept
{
    return _r.textFormatAxes[italic][bold];
}

AtlasEngine::Cell* AtlasEngine::_getCell(u16 x, u16 y) noexcept
{
    assert(x < _r.cellCount.x);
    assert(y < _r.cellCount.y);
    return _r.cells.data() + static_cast<size_t>(_r.cellCount.x) * y + x;
}

void AtlasEngine::_setCellFlags(SMALL_RECT coords, CellFlags mask, CellFlags bits) noexcept
{
    assert(coords.Left <= coords.Right);
    assert(coords.Top <= coords.Bottom);
    assert(coords.Right <= _r.cellCount.x);
    assert(coords.Bottom <= _r.cellCount.y);

    const auto filter = ~mask;
    const auto width = static_cast<size_t>(coords.Right) - coords.Left;
    const auto height = static_cast<size_t>(coords.Bottom) - coords.Top;
    const auto stride = static_cast<size_t>(_r.cellCount.x);
    auto row = _r.cells.data() + static_cast<size_t>(_r.cellCount.x) * coords.Top + coords.Left;
    const auto end = row + height * stride;

    for (; row != end; row += stride)
    {
        const auto dataEnd = row + width;
        for (auto data = row; data != dataEnd; ++data)
        {
            const auto current = data->flags;
            data->flags = (current & filter) | bits;
        }
    }
}

AtlasEngine::u16x2 AtlasEngine::_allocateAtlasTile() noexcept
{
    const auto ret = _r.atlasPosition;

    _r.atlasPosition.x += _r.cellSize.x;
    if (_r.atlasPosition.x >= _r.atlasSizeInPixelLimit.x)
    {
        _r.atlasPosition.x = 0;
        _r.atlasPosition.y += _r.cellSize.y;
        if (_r.atlasPosition.y >= _r.atlasSizeInPixelLimit.y)
        {
            _r.atlasPosition.x = _r.cellSize.x;
            _r.atlasPosition.y = 0;
            showOOMWarning();
        }
    }

    return ret;
}

void AtlasEngine::_flushBufferLine()
{
    if (_api.bufferLine.empty())
    {
        return;
    }

    const auto cleanup = wil::scope_exit([this]() noexcept {
        _api.bufferLine.clear();
        _api.bufferLineColumn.clear();
    });

    // This would seriously blow us up otherwise.
    Expects(_api.bufferLineColumn.size() == _api.bufferLine.size() + 1);

    // NOTE:
    // This entire function is one huge hack to see if it works.

    // UH OH UNICODE MADNESS AHEAD
    //
    // # What do we want?
    //
    // Segment a line of text (_api.bufferLine) into unicode "clusters".
    // Each cluster is one "whole" glyph with diacritics, ligatures, zero width joiners
    // and whatever else, that should be cached as a whole in our texture atlas.
    //
    // # How do we get that?
    //
    // ## The unfortunate preface
    //
    // DirectWrite can be "reluctant" to segment text into clusters and I found no API which offers simply that.
    // What it offers are a large number of low level APIs that can sort of be used in combination to do this.
    // The resulting text parsing is very slow unfortunately, consuming up to 95% of rendering time in extreme cases.
    //
    // ## The actual approach
    //
    // DirectWrite has 2 APIs which can segment text properly (including ligatures and zero width joiners):
    // * IDWriteTextAnalyzer1::GetTextComplexity
    // * IDWriteTextAnalyzer::GetGlyphs
    //
    // Both APIs require us to attain an IDWriteFontFace as the functions themselves don't handle font fallback.
    // This forces us to call IDWriteFontFallback::MapCharacters first.
    //
    // Additionally IDWriteTextAnalyzer::GetGlyphs requires an instance of DWRITE_SCRIPT_ANALYSIS,
    // which can only be attained by running IDWriteTextAnalyzer::AnalyzeScript first.
    //
    // Font fallback with IDWriteFontFallback::MapCharacters is very slow.

    const auto textFormat = _getTextFormat(_api.attributes.bold, _api.attributes.italic);
    const auto& textFormatAxis = _getTextFormatAxis(_api.attributes.bold, _api.attributes.italic);

    TextAnalyzer atlasAnalyzer{ _api.bufferLine, _api.analysisResults };

    wil::com_ptr<IDWriteFontCollection> fontCollection;
    THROW_IF_FAILED(textFormat->GetFontCollection(fontCollection.addressof()));

    wil::com_ptr<IDWriteFontFace> mappedFontFace;

#pragma warning(suppress : 26494) // Variable 'mappedEnd' is uninitialized. Always initialize an object (type.5).
    for (u32 idx = 0, mappedEnd; idx < _api.bufferLine.size(); idx = mappedEnd)
    {
        if (_sr.systemFontFallback)
        {
            auto scale = 1.0f;
            u32 mappedLength = 0;

            if (textFormatAxis)
            {
                wil::com_ptr<IDWriteFontFace5> fontFace5;
                THROW_IF_FAILED(_sr.systemFontFallback.query<IDWriteFontFallback1>()->MapCharacters(
                    /* analysisSource */ &atlasAnalyzer,
                    /* textPosition */ idx,
                    /* textLength */ gsl::narrow_cast<u32>(_api.bufferLine.size()) - idx,
                    /* baseFontCollection */ fontCollection.get(),
                    /* baseFamilyName */ _api.fontMetrics.fontName.get(),
                    /* fontAxisValues */ textFormatAxis.data(),
                    /* fontAxisValueCount */ gsl::narrow_cast<u32>(textFormatAxis.size()),
                    /* mappedLength */ &mappedLength,
                    /* scale */ &scale,
                    /* mappedFontFace */ fontFace5.put()));
                mappedFontFace = std::move(fontFace5);
            }
            else
            {
                const auto baseWeight = _api.attributes.bold ? DWRITE_FONT_WEIGHT_BOLD : static_cast<DWRITE_FONT_WEIGHT>(_api.fontMetrics.fontWeight);
                const auto baseStyle = _api.attributes.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
                wil::com_ptr<IDWriteFont> font;

                THROW_IF_FAILED(_sr.systemFontFallback->MapCharacters(
                    /* analysisSource     */ &atlasAnalyzer,
                    /* textPosition       */ idx,
                    /* textLength         */ gsl::narrow_cast<u32>(_api.bufferLine.size()) - idx,
                    /* baseFontCollection */ fontCollection.get(),
                    /* baseFamilyName     */ _api.fontMetrics.fontName.get(),
                    /* baseWeight         */ baseWeight,
                    /* baseStyle          */ baseStyle,
                    /* baseStretch        */ DWRITE_FONT_STRETCH_NORMAL,
                    /* mappedLength       */ &mappedLength,
                    /* mappedFont         */ font.addressof(),
                    /* scale              */ &scale));

                mappedFontFace.reset();
                if (font)
                {
                    THROW_IF_FAILED(font->CreateFontFace(mappedFontFace.addressof()));
                }
            }

            mappedEnd = idx + mappedLength;

            if (!mappedFontFace)
            {
                // Task: Replace all characters in this range with unicode replacement characters.
                // Input (where "n" is a narrow and "ww" is a wide character):
                //    _api.bufferLine       = "nwwnnw"
                //    _api.bufferLineColumn = {0, 1, 1, 2, 3, 4, 4, 5}
                //                             n  w  w  n  n  w  w
                // Solution:
                //   Iterate through bufferLineColumn until the value changes, because this indicates we passed over a
                //   complete (narrow or wide) cell. To do so we'll use col1 (previous column) and col2 (next column).
                //   Then we emit a replacement character by telling _emplaceGlyph that this range has no font face.
                auto pos1 = idx;
                auto col1 = _api.bufferLineColumn[pos1];
                for (auto pos2 = idx + 1; pos2 <= mappedEnd; ++pos2)
                {
                    if (const auto col2 = _api.bufferLineColumn[pos2]; col1 != col2)
                    {
                        _emplaceGlyph(nullptr, pos1, pos2);
                        pos1 = pos2;
                        col1 = col2;
                    }
                }

                continue;
            }
        }
        else
        {
            if (!mappedFontFace)
            {
                const auto baseWeight = _api.attributes.bold ? DWRITE_FONT_WEIGHT_BOLD : static_cast<DWRITE_FONT_WEIGHT>(_api.fontMetrics.fontWeight);
                const auto baseStyle = _api.attributes.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

                wil::com_ptr<IDWriteFontFamily> fontFamily;
                THROW_IF_FAILED(fontCollection->GetFontFamily(0, fontFamily.addressof()));

                wil::com_ptr<IDWriteFont> font;
                THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(baseWeight, DWRITE_FONT_STRETCH_NORMAL, baseStyle, font.addressof()));

                THROW_IF_FAILED(font->CreateFontFace(mappedFontFace.put()));
            }

            mappedEnd = gsl::narrow_cast<u32>(_api.bufferLine.size());
        }

        // We can reuse idx here, as it'll be reset to "idx = mappedEnd" in the outer loop anyways.
        for (u32 complexityLength = 0; idx < mappedEnd; idx += complexityLength)
        {
            BOOL isTextSimple;
            THROW_IF_FAILED(_sr.textAnalyzer->GetTextComplexity(_api.bufferLine.data() + idx, mappedEnd - idx, mappedFontFace.get(), &isTextSimple, &complexityLength, _api.glyphIndices.data()));

            if (isTextSimple)
            {
                for (size_t i = 0; i < complexityLength; ++i)
                {
                    _emplaceGlyph(mappedFontFace.get(), idx + i, idx + i + 1u);
                }
            }
            else
            {
                _api.analysisResults.clear();
                THROW_IF_FAILED(_sr.textAnalyzer->AnalyzeScript(&atlasAnalyzer, idx, complexityLength, &atlasAnalyzer));
                //_sr.textAnalyzer->AnalyzeBidi(&atlasAnalyzer, idx, complexityLength, &atlasAnalyzer);

                for (const auto& a : _api.analysisResults)
                {
                    DWRITE_SCRIPT_ANALYSIS scriptAnalysis{ a.script, static_cast<DWRITE_SCRIPT_SHAPES>(a.shapes) };
                    u32 actualGlyphCount = 0;

#pragma warning(push)
#pragma warning(disable : 26494) // Variable '...' is uninitialized. Always initialize an object (type.5).
                    // None of these variables need to be initialized.
                    // features/featureRangeLengths are marked _In_reads_opt_(featureRanges).
                    // featureRanges is only > 0 when we also initialize all these variables.
                    DWRITE_TYPOGRAPHIC_FEATURES feature;
                    const DWRITE_TYPOGRAPHIC_FEATURES* features;
                    u32 featureRangeLengths;
#pragma warning(pop)
                    u32 featureRanges = 0;

                    if (!_api.fontFeatures.empty())
                    {
                        feature.features = _api.fontFeatures.data();
                        feature.featureCount = gsl::narrow_cast<u32>(_api.fontFeatures.size());
                        features = &feature;
                        featureRangeLengths = a.textLength;
                        featureRanges = 1;
                    }

                    if (_api.clusterMap.size() < a.textLength)
                    {
                        _api.clusterMap = Buffer<u16>{ a.textLength };
                        _api.textProps = Buffer<DWRITE_SHAPING_TEXT_PROPERTIES>{ a.textLength };
                    }

                    for (auto retry = 0;;)
                    {
                        const auto hr = _sr.textAnalyzer->GetGlyphs(
                            /* textString          */ _api.bufferLine.data() + a.textPosition,
                            /* textLength          */ a.textLength,
                            /* fontFace            */ mappedFontFace.get(),
                            /* isSideways          */ false,
                            /* isRightToLeft       */ a.bidiLevel & 1,
                            /* scriptAnalysis      */ &scriptAnalysis,
                            /* localeName          */ nullptr,
                            /* numberSubstitution  */ nullptr,
                            /* features            */ &features,
                            /* featureRangeLengths */ &featureRangeLengths,
                            /* featureRanges       */ featureRanges,
                            /* maxGlyphCount       */ gsl::narrow_cast<u32>(_api.glyphProps.size()),
                            /* clusterMap          */ _api.clusterMap.data(),
                            /* textProps           */ _api.textProps.data(),
                            /* glyphIndices        */ _api.glyphIndices.data(),
                            /* glyphProps          */ _api.glyphProps.data(),
                            /* actualGlyphCount    */ &actualGlyphCount);

                        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8)
                        {
                            // Grow factor 1.5x.
                            auto size = _api.glyphProps.size();
                            size = size + (size >> 1);
                            // Overflow check.
                            Expects(size > _api.glyphProps.size());
                            _api.glyphIndices = Buffer<u16>{ size };
                            _api.glyphProps = Buffer<DWRITE_SHAPING_GLYPH_PROPERTIES>(size);
                            continue;
                        }

                        THROW_IF_FAILED(hr);
                        break;
                    }

                    if (_api.glyphAdvances.size() < actualGlyphCount)
                    {
                        // Grow the buffer by at least 1.5x and at least of `actualGlyphCount` items.
                        // The 1.5x growth ensures we don't reallocate every time we need 1 more slot.
                        auto size = _api.glyphAdvances.size();
                        size = size + (size >> 1);
                        size = std::max<size_t>(size, actualGlyphCount);
                        _api.glyphAdvances = Buffer<f32>{ size };
                        _api.glyphOffsets = Buffer<DWRITE_GLYPH_OFFSET>{ size };
                    }

                    THROW_IF_FAILED(_sr.textAnalyzer->GetGlyphPlacements(
                        /* textString          */ _api.bufferLine.data() + a.textPosition,
                        /* clusterMap          */ _api.clusterMap.data(),
                        /* textProps           */ _api.textProps.data(),
                        /* textLength          */ a.textLength,
                        /* glyphIndices        */ _api.glyphIndices.data(),
                        /* glyphProps          */ _api.glyphProps.data(),
                        /* glyphCount          */ actualGlyphCount,
                        /* fontFace            */ mappedFontFace.get(),
                        /* fontEmSize          */ _api.fontMetrics.fontSizeInDIP,
                        /* isSideways          */ false,
                        /* isRightToLeft       */ a.bidiLevel & 1,
                        /* scriptAnalysis      */ &scriptAnalysis,
                        /* localeName          */ nullptr,
                        /* features            */ &features,
                        /* featureRangeLengths */ &featureRangeLengths,
                        /* featureRanges       */ featureRanges,
                        /* glyphAdvances       */ _api.glyphAdvances.data(),
                        /* glyphOffsets        */ _api.glyphOffsets.data()));

                    _api.textProps[a.textLength - 1].canBreakShapingAfter = 1;

                    size_t beg = 0;
                    for (size_t i = 0; i < a.textLength; ++i)
                    {
                        if (_api.textProps[i].canBreakShapingAfter)
                        {
                            _emplaceGlyph(mappedFontFace.get(), a.textPosition + beg, a.textPosition + i + 1);
                            beg = i + 1;
                        }
                    }
                }
            }
        }
    }
}

void AtlasEngine::_emplaceGlyph(IDWriteFontFace* fontFace, size_t bufferPos1, size_t bufferPos2)
{
    static constexpr auto replacement = L'\uFFFD';

    // This would seriously blow us up otherwise.
    Expects(bufferPos1 < bufferPos2 && bufferPos2 <= _api.bufferLine.size());

    const auto chars = fontFace ? &_api.bufferLine[bufferPos1] : &replacement;
    const auto charCount = fontFace ? bufferPos2 - bufferPos1 : 1;

    // _flushBufferLine() ensures that bufferLineColumn.size() > bufferLine.size().
    const auto x1 = _api.bufferLineColumn[bufferPos1];
    const auto x2 = _api.bufferLineColumn[bufferPos2];

    Expects(x1 < x2 && x2 <= _api.cellCount.x);

    const u16 cellCount = x2 - x1;

    auto attributes = _api.attributes;
    attributes.cellCount = cellCount;

    const auto [it, inserted] = _r.glyphs.emplace(std::piecewise_construct, std::forward_as_tuple(attributes, gsl::narrow<u16>(charCount), chars), std::forward_as_tuple());
    const auto& key = it->first;
    auto& value = it->second;

    if (inserted)
    {
        // Do fonts exist *in practice* which contain both colored and uncolored glyphs? I'm pretty sure...
        // However doing it properly means using either of:
        // * IDWriteFactory2::TranslateColorGlyphRun
        // * IDWriteFactory4::TranslateColorGlyphRun
        // * IDWriteFontFace4::GetGlyphImageData
        //
        // For the first two I wonder how one is supposed to restitch the 27 required parameters from a
        // partial (!) text range returned by GetGlyphs(). Our caller breaks the GetGlyphs() result up
        // into runs of characters up until the first canBreakShapingAfter == true after all.
        // There's no documentation for it and I'm sure I'd mess it up.
        // For very obvious reasons I didn't want to write this code.
        //
        // IDWriteFontFace4::GetGlyphImageData seems like the best approach and DirectWrite uses the
        // same code that GetGlyphImageData uses to implement TranslateColorGlyphRun (well partially).
        // However, it's a heavy operation and parses the font file on disk on every call (TranslateColorGlyphRun doesn't).
        // For less obvious reasons I didn't want to write this code either.
        //
        // So this is a job for future me/someone.
        // Bonus points for doing it without impacting performance.
        auto flags = CellFlags::None;
        if (fontFace)
        {
            const auto fontFace2 = wil::try_com_query<IDWriteFontFace2>(fontFace);
            WI_SetFlagIf(flags, CellFlags::ColoredGlyph, fontFace2 && fontFace2->IsColorFont());
        }

        const auto coords = value.initialize(flags, cellCount);
        for (u16 i = 0; i < cellCount; ++i)
        {
            coords[i] = _allocateAtlasTile();
        }

        _r.glyphQueue.push_back(AtlasQueueItem{ &key, &value });
        _r.maxEncounteredCellCount = std::max(_r.maxEncounteredCellCount, cellCount);
    }

    const auto valueData = value.data();
    const auto coords = &valueData->coords[0];
    const auto data = _getCell(x1, _api.lastPaintBufferLineCoord.y);

    for (u32 i = 0; i < cellCount; ++i)
    {
        data[i].tileIndex = coords[i];
        // We should apply the column color and flags from each column (instead
        // of copying them from the x1) so that ligatures can appear in multiple
        // colors with different line styles.
        data[i].flags = valueData->flags | _api.bufferLineMetadata[static_cast<size_t>(x1) + i].flags;
        data[i].color = _api.bufferLineMetadata[static_cast<size_t>(x1) + i].colors;
    }
}
