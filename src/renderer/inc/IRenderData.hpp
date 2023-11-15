// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/generational.h>

#include "CSSLengthPercentage.h"
#include "../../buffer/out/textBuffer.hpp"

class Cursor;

namespace Microsoft::Console::Render
{
    enum class CursorStyle
    {
        Invert = 0,
        Color,
    };

    enum class TextAntialiasMode
    {
        Default = 0,
        ClearType,
        Grayscale,
        Aliased,
    };

    struct TargetSettings
    {
        COLORREF paddingColor = 0;
        bool forceFullRepaint = false;
        bool transparentBackground = false;
        bool softwareRendering = false;
    };

    struct FontSettings
    {
        std::wstring faceName;
        int family = 0;
        int weight = 0;
        int codePage = 0;
        float fontSize = 0;
        CSSLengthPercentage cellWidth;
        CSSLengthPercentage cellHeight;
        std::unordered_map<std::wstring_view, uint32_t> features;
        std::unordered_map<std::wstring_view, float> axes;
        TextAntialiasMode antialiasingMode = TextAntialiasMode::Default;
        float dpi = 0;
    };

    struct CursorSettings
    {
        CursorType type = CursorType::Legacy;
        CursorStyle style = CursorStyle::Invert;
        COLORREF color = 0xffffffff;
        float heightPercentage = 0.2f;
        float widthInDIP = 1.0f;
    };

    struct SelectionSettings
    {
        COLORREF selectionColor = 0x7fffffff;
    };

    struct ShaderSettings
    {
        std::wstring shaderPath;
        bool retroTerminalEffect = false;
    };

    struct Settings
    {
        til::generational<TargetSettings> target;
        til::generational<FontSettings> font;
        til::generational<CursorSettings> cursor;
        til::generational<SelectionSettings> selection;
        til::generational<ShaderSettings> shader;

        til::size targetSizeInPixel;
    };

    struct RenderingLayer
    {
        TextBuffer* source;
        til::rect sourceRegion;
        til::point targetOrigin;
    };

    struct RenderData
    {
        til::generational<Settings> settings;

        std::vector<til::rect> selections;
        uint16_t hoveredHyperlinkId = 0;
        
        std::vector<RenderingLayer> layers;
    };

    struct RenderingPayload
    {
        til::generational<Settings> settings;

        std::vector<til::rect> selections;
        uint16_t hoveredHyperlinkId = 0;

        TextBuffer buffer;
    };

    struct IRenderData
    {
        virtual ~IRenderData() = default;

        virtual void UpdateRenderData(RenderData& data) = 0;
    };
}
