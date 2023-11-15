// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render
{
    class GdiEngine final : public IRenderEngine
    {
    public:
        GdiEngine();
        ~GdiEngine() override;

        void WaitUntilCanRender() noexcept override;
        void Render(const RenderingPayload& payload) override;

    private:
    };
}
