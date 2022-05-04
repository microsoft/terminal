/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IWindowMetrics.hpp

Abstract:
- Defines methods that return the maximum and minimum dimensions permissible for
  the console window.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IWindowMetrics
    {
    public:
        virtual ~IWindowMetrics() = default;
        virtual RECT GetMinClientRectInPixels() = 0;
        virtual RECT GetMaxClientRectInPixels() = 0;
    };
}
