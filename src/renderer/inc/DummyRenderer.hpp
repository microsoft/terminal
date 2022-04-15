/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DummyRenderer.hpp

Abstract:
- Provides a minimal instantiation of the Renderer class.
    This is needed for some tests, where certain objects need a reference to a
    Renderer
--*/

#pragma once
#include "../base/renderer.hpp"

class DummyRenderer final : public Microsoft::Console::Render::Renderer
{
public:
    DummyRenderer(Microsoft::Console::Render::IRenderData* pData = nullptr) :
        Microsoft::Console::Render::Renderer(_renderSettings, pData, nullptr, 0, nullptr) {}

    Microsoft::Console::Render::RenderSettings _renderSettings;
};
