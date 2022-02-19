/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DummyRenderTarget.hpp

Abstract:
- Provides a minimal instantiation of the Renderer class.
    This is needed for some tests, where certain objects need a reference to a
    Renderer

Author(s):
- Mike Griese (migrie) Nov 2018
--*/

#pragma once
#include "../base/renderer.hpp"

class DummyRenderTarget final : public Microsoft::Console::Render::Renderer
{
public:
    DummyRenderTarget(Microsoft::Console::Render::IRenderData* pData = nullptr) :
        Microsoft::Console::Render::Renderer(_renderSettings, pData, nullptr, 0, nullptr) {}

private:
    Microsoft::Console::Render::RenderSettings _renderSettings;
};
