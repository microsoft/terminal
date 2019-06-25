/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DispatchCommon.hpp

Abstract:
- Defines a number of common functions and enums whose implementation is the
    same in both the AdaptDispatch and the InteractDispatch.

Author(s):
- Mike Griese (migrie) 11 Oct 2017
--*/

#pragma once

#include "conGetSet.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class DispatchCommon final
    {
    public:
        static bool s_ResizeWindow(ConGetSet& conApi,
                                   const unsigned short usWidth,
                                   const unsigned short usHeight);

        static bool s_RefreshWindow(ConGetSet& conApi);

        static bool s_SuppressResizeRepaint(ConGetSet& conApi);
    };
}
