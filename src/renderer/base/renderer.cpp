// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "renderer.hpp"

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

Renderer::Renderer(IRenderData* renderData) :
    _renderData{ renderData }
{
}

Renderer::~Renderer()
{
}

void Renderer::AddRenderEngine(IRenderEngine* const renderEngine)
{
    _renderEngines.emplace_back(renderEngine);
}

void Renderer::RemoveRenderEngine(IRenderEngine* const renderEngine)
{
    _renderEngines.erase(std::remove(_renderEngines.begin(), _renderEngines.end(), renderEngine), _renderEngines.end());
}

void Renderer::TriggerRedraw() noexcept
{
}

bool Renderer::IsGlyphWideByFont(const std::wstring_view glyph)
{
    return glyph.size() > 1;
}

void Renderer::SetRendererEnteredErrorStateCallback(std::function<void()> callback)
{
    _errorStateCallback = std::move(callback);
}

void Renderer::ResetErrorStateAndResume()
{
}
