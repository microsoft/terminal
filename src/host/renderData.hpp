/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- renderData.hpp

Abstract:
- This method provides an interface for rendering the final display based on the current console state

Author(s):
- Michael Niksa (miniksa) Nov 2015
--*/

#pragma once

#include "..\renderer\inc\IRenderData.hpp"
#include "..\types\IUiaData.h"

class RenderData final :
    public Microsoft::Console::Render::IRenderData,
    public Microsoft::Console::Types::IUiaData
{
public:
#pragma region BaseData
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    const TextBuffer& GetTextBuffer() noexcept override;
    const FontInfo& GetFontInfo() noexcept override;

    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override;

    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;
#pragma endregion

#pragma region IRenderData
    const TextAttribute GetDefaultBrushColors() noexcept override;

    const COLORREF GetForegroundColor(const TextAttribute& attr) const noexcept override;
    const COLORREF GetBackgroundColor(const TextAttribute& attr) const noexcept override;

    COORD GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    COLORREF GetCursorColor() const noexcept override;
    bool IsCursorDoubleWidth() const noexcept override;

    const std::vector<Microsoft::Console::Render::RenderOverlay> GetOverlays() const noexcept override;

    const bool IsGridLineDrawingAllowed() noexcept override;

    const std::wstring GetConsoleTitle() const noexcept override;
#pragma endregion

#pragma region IUiaData
    const bool IsSelectionActive() const override;
    void ClearSelection() override;
    void SelectNewRegion(const COORD coordStart, const COORD coordEnd) override;
    const COORD GetSelectionAnchor() const;
    void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute attr);
#pragma endregion
};
