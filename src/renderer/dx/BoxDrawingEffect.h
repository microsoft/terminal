// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "IBoxDrawingEffect_h.h"

namespace Microsoft::Console::Render
{
    class BoxDrawingEffect : public ::Microsoft::WRL::RuntimeClass<::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom | ::Microsoft::WRL::InhibitFtmBase>, IBoxDrawingEffect>
    {
    public:

    protected:
        
    private:

#ifdef UNIT_TESTING
    public:
        BoxDrawingEffect() = default;

        friend class BoxDrawingEffectTests;
#endif
    };
}
