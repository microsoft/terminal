// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "IRenderData.hpp"

namespace Microsoft::Console::Render
{
    class __declspec(novtable) IRenderEngine
    {
    public:
        virtual ~IRenderEngine() = default;

        virtual void WaitUntilCanRender() = 0;
        virtual void Render(RenderingPayload& payload) = 0;
    };
}
