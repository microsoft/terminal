/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Renderer.hpp

Abstract:
- This is the definition of our renderer.
- It provides interfaces for the console application to notify when various portions of the console state have changed and need to be redrawn.
- It requires a data interface to fetch relevant console structures required for drawing and a drawing engine target for output.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "../inc/IRenderEngine.hpp"
#include "../inc/RenderSettings.hpp"

#include "thread.hpp"

#include "../../buffer/out/textBuffer.hpp"

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalCoreUnitTests
{
    class ConptyRoundtripTests;
};
#endif

namespace Microsoft::Console::Render
{
    class Renderer
    {
    public:
        Renderer(const RenderSettings& renderSettings,
                 IRenderData* pData,
                 _In_reads_(cEngines) IRenderEngine** const pEngine,
                 const size_t cEngines,
                 std::unique_ptr<RenderThread> thread);

        ~Renderer();

        [[nodiscard]] HRESULT PaintFrame();

        void NotifyPaintFrame() noexcept;
        void TriggerSystemRedraw(const til::rect* const prcDirtyClient);
        void TriggerRedraw(const Microsoft::Console::Types::Viewport& region);
        void TriggerRedraw(const til::point* const pcoord);
        void TriggerRedrawCursor(const til::point* const pcoord);
        void TriggerRedrawAll(const bool backgroundChanged = false, const bool frameChanged = false);
        void TriggerTeardown() noexcept;
        void TriggerSelection();
        void UpdateSoftFont(const std::span<const uint16_t> bitPattern, const til::size cellSize, const size_t centeringHint);
        bool IsGlyphWideByFont(const std::wstring_view& glyph);
        void EnablePainting();
        void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs);
        void WaitUntilCanRender();
        void AddRenderEngine(_In_ IRenderEngine* const pEngine);
        void RemoveRenderEngine(_In_ IRenderEngine* const pEngine);

    private:
        static bool s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar);

        const RenderSettings& _renderSettings;
        std::array<IRenderEngine*, 2> _engines{};
        IRenderData* _pData = nullptr; // Non-ownership pointer
        std::unique_ptr<RenderThread> _pThread;
        static constexpr size_t _firstSoftFontChar = 0xEF20;
        size_t _lastSoftFontChar = 0;
        Microsoft::Console::Types::Viewport _viewport;
        std::function<void()> _pfnBackgroundColorChanged;
        std::function<void()> _pfnFrameColorChanged;
        std::function<void()> _pfnRendererEnteredErrorState;
        bool _destructing = false;
        bool _forceUpdateViewport = false;

#ifdef UNIT_TESTING
        friend class ConptyOutputTests;
        friend class TerminalCoreUnitTests::ConptyRoundtripTests;
#endif
    };
}
