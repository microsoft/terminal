// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render
{
    class Renderer
    {
    public:
        Renderer(IRenderData* renderData);
        ~Renderer();
        
        void AddRenderEngine(_In_ IRenderEngine* pEngine);
        void RemoveRenderEngine(_In_ IRenderEngine* pEngine);

        void TriggerRedraw() noexcept;

        bool IsGlyphWideByFont(std::wstring_view glyph);
        void SetRendererEnteredErrorStateCallback(std::function<void()> callback);
        void ResetErrorStateAndResume();

    private:
        IRenderData* _renderData;
        til::small_vector<IRenderEngine*, 2> _renderEngines;
        std::function<void()> _errorStateCallback;
    };
}
