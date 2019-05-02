/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WindowMetrics.hpp

Abstract:
- OneCore implementation of the IWindowMetrics interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "..\inc\IWindowMetrics.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class WindowMetrics sealed : public IWindowMetrics
    {
    public:
        RECT GetMinClientRectInPixels();
        RECT GetMaxClientRectInPixels();
    };
}
