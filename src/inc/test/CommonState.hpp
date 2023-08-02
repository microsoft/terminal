/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- CommonState.hpp

Abstract:
- This represents common boilerplate state setup required for unit tests to run

Author(s):
- Michael Niksa (miniksa) 18-Jun-2014
- Paul Campbell (paulcam) 18-Jun-2014

Revision History:
- Transformed to header-only class so it can be included by multiple
unit testing projects in the codebase without a bunch of overhead.
--*/

#pragma once

#include "../host/globals.h"
#include "../host/inputReadHandleData.h"
#include "../interactivity/inc/ServiceLocator.hpp"

class CommonState
{
public:
    static const til::CoordType s_csWindowWidth = 80;
    static const til::CoordType s_csWindowHeight = 80;
    static const til::CoordType s_csBufferWidth = 80;
    static const til::CoordType s_csBufferHeight = 300;

    CommonState() :
        m_heap(GetProcessHeap()),
        m_hrTextBufferInfo(E_FAIL),
        m_pFontInfo(nullptr),
        m_backupTextBufferInfo(),
        m_readHandle(nullptr)
    {
    }

    ~CommonState()
    {
        m_heap = nullptr;
    }

    void InitEvents()
    {
        Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().hInputEvent.create(wil::EventOptions::ManualReset);
    }

    void PrepareReadHandle()
    {
        m_readHandle = std::make_unique<INPUT_READ_HANDLE_DATA>();
    }

    void CleanupReadHandle()
    {
        m_readHandle.reset(nullptr);
    }

    void PrepareGlobalFont(const til::size coordFontSize = { 8, 12 })
    {
        m_pFontInfo = new FontInfo(L"Consolas", 0, 0, coordFontSize, 0);
    }

    void CleanupGlobalFont()
    {
        if (m_pFontInfo != nullptr)
        {
            delete m_pFontInfo;
        }
    }

    void PrepareGlobalRenderer()
    {
        Globals& g = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();
        g.pRender = new Microsoft::Console::Render::Renderer(gci.GetRenderSettings(), &gci.renderData, nullptr, 0, nullptr);
    }

    void CleanupGlobalRenderer()
    {
        Globals& g = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        delete g.pRender;
        g.pRender = nullptr;
    }

    void PrepareGlobalScreenBuffer(const til::CoordType viewWidth = s_csWindowWidth,
                                   const til::CoordType viewHeight = s_csWindowHeight,
                                   const til::CoordType bufferWidth = s_csBufferWidth,
                                   const til::CoordType bufferHeight = s_csBufferHeight)
    {
        Globals& g = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();
        til::size coordWindowSize;
        coordWindowSize.width = viewWidth;
        coordWindowSize.height = viewHeight;

        til::size coordScreenBufferSize;
        coordScreenBufferSize.width = bufferWidth;
        coordScreenBufferSize.height = bufferHeight;

        UINT uiCursorSize = 12;

        THROW_IF_FAILED(SCREEN_INFORMATION::CreateInstance(coordWindowSize,
                                                           *m_pFontInfo,
                                                           coordScreenBufferSize,
                                                           TextAttribute{},
                                                           TextAttribute{ FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_RED },
                                                           uiCursorSize,
                                                           &gci.pCurrentScreenBuffer));

        // If we have a renderer, we need to call EnablePainting to initialize
        // the viewport. If not, we mark the text buffer as inactive so that it
        // doesn't try to trigger a redraw on a nonexistent renderer.
        if (g.pRender)
        {
            g.pRender->EnablePainting();
        }
        else
        {
            gci.pCurrentScreenBuffer->_textBuffer->SetAsActiveBuffer(false);
        }
    }

    void CleanupGlobalScreenBuffer()
    {
        const CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        delete gci.pCurrentScreenBuffer;
    }

    void PrepareGlobalInputBuffer()
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.pInputBuffer = new InputBuffer();
    }

    void CleanupGlobalInputBuffer()
    {
        const CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        delete gci.pInputBuffer;
    }

    void PrepareCookedReadData(const std::wstring_view initialData = {})
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        auto* readData = new COOKED_READ_DATA(gci.pInputBuffer,
                                              m_readHandle.get(),
                                              gci.GetActiveOutputBuffer(),
                                              0,
                                              nullptr,
                                              0,
                                              L"",
                                              initialData,
                                              nullptr);
        gci.SetCookedReadData(readData);
    }

    void CleanupCookedReadData()
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        delete &gci.CookedReadData();
        gci.SetCookedReadData(nullptr);
    }

    void PrepareNewTextBufferInfo(const bool useDefaultAttributes = false,
                                  const til::CoordType bufferWidth = s_csBufferWidth,
                                  const til::CoordType bufferHeight = s_csBufferHeight)
    {
        Globals& g = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();
        til::size coordScreenBufferSize;
        coordScreenBufferSize.width = bufferWidth;
        coordScreenBufferSize.height = bufferHeight;

        UINT uiCursorSize = 12;

        auto initialAttributes = useDefaultAttributes ? TextAttribute{} :
                                                        TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY };

        m_backupTextBufferInfo.swap(gci.pCurrentScreenBuffer->_textBuffer);
        try
        {
            std::unique_ptr<TextBuffer> textBuffer = std::make_unique<TextBuffer>(coordScreenBufferSize,
                                                                                  initialAttributes,
                                                                                  uiCursorSize,
                                                                                  true,
                                                                                  *g.pRender);
            if (textBuffer.get() == nullptr)
            {
                m_hrTextBufferInfo = E_OUTOFMEMORY;
            }
            else
            {
                m_hrTextBufferInfo = S_OK;
            }
            gci.pCurrentScreenBuffer->_textBuffer.swap(textBuffer);

            // If we have a renderer, we need to call EnablePainting to initialize
            // the viewport. If not, we mark the text buffer as inactive so that it
            // doesn't try to trigger a redraw on a nonexistent renderer.
            if (g.pRender)
            {
                g.pRender->EnablePainting();
            }
            else
            {
                gci.pCurrentScreenBuffer->_textBuffer->SetAsActiveBuffer(false);
            }
        }
        catch (...)
        {
            m_hrTextBufferInfo = wil::ResultFromCaughtException();
        }
    }

    void CleanupNewTextBufferInfo()
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        VERIFY_IS_TRUE(gci.HasActiveOutputBuffer());

        gci.pCurrentScreenBuffer->_textBuffer.swap(m_backupTextBufferInfo);
    }

    void FillTextBuffer()
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        // fill with some assorted text that doesn't consume the whole row
        const til::CoordType cRowsToFill = 4;

        VERIFY_IS_TRUE(gci.HasActiveOutputBuffer());

        TextBuffer& textBuffer = gci.GetActiveOutputBuffer().GetTextBuffer();

        for (til::CoordType iRow = 0; iRow < cRowsToFill; iRow++)
        {
            ROW& row = textBuffer.GetRowByOffset(iRow);
            FillRow(&row, iRow & 1);
        }

        textBuffer.GetCursor().SetYPosition(cRowsToFill);
    }

    [[nodiscard]] HRESULT GetTextBufferInfoInitResult()
    {
        return m_hrTextBufferInfo;
    }

private:
    HANDLE m_heap;
    HRESULT m_hrTextBufferInfo;
    FontInfo* m_pFontInfo;
    std::unique_ptr<TextBuffer> m_backupTextBufferInfo;
    std::unique_ptr<INPUT_READ_HANDLE_DATA> m_readHandle;

    void FillRow(ROW* pRow, bool wrapForced)
    {
        // fill a row
        // 9 characters, 6 spaces. 15 total
        // か = \x304b
        // き = \x304d

        uint16_t column = 0;
        for (const auto& ch : std::wstring_view{ L"AB\u304bC\u304dDE      " })
        {
            const uint16_t width = ch >= 0x80 ? 2 : 1;
            pRow->ReplaceCharacters(column, width, { &ch, 1 });
            column += width;
        }

        // A = bright red on dark gray
        // This string starts at index 0
        auto Attr = TextAttribute(FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY);
        pRow->SetAttrToEnd(0, Attr);

        // BかC = dark gold on bright blue
        // This string starts at index 1
        Attr = TextAttribute(FOREGROUND_RED | FOREGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
        pRow->SetAttrToEnd(1, Attr);

        // き = bright white on dark purple
        // This string starts at index 5
        Attr = TextAttribute(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE);
        pRow->SetAttrToEnd(5, Attr);

        // DE = black on dark green
        // This string starts at index 7
        Attr = TextAttribute(BACKGROUND_GREEN);
        pRow->SetAttrToEnd(7, Attr);

        // odd rows forced a wrap
        if (wrapForced)
        {
            pRow->SetWrapForced(true);
        }
        else
        {
            pRow->SetWrapForced(false);
        }
    }
};
