// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "IRenderData.hpp"

namespace Microsoft::Console::Render
{
    class IRenderEngine
    {
    public:
        virtual ~IRenderEngine() = default;

        virtual void WaitUntilCanRender() noexcept = 0;
        virtual void Render(const RenderingPayload& payload) = 0;
    };
}
