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
#include "DispatchTypes.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class DispatchCommon final
    {
    public:
        static bool s_ResizeWindow(ConGetSet& conApi,
                                   const size_t width,
                                   const size_t height);

        static bool s_RefreshWindow(ConGetSet& conApi);

        static bool s_SuppressResizeRepaint(ConGetSet& conApi);

        static bool s_EraseInDisplay(ConGetSet& conApi, const DispatchTypes::EraseType eraseType);
        static bool s_EraseAll(ConGetSet& conApi);
        static bool s_EraseScrollback(ConGetSet& conApi);
        static bool s_EraseSingleLineHelper(ConGetSet& conApi,
                                            const CONSOLE_SCREEN_BUFFER_INFOEX& csbiex,
                                            const DispatchTypes::EraseType eraseType,
                                            const size_t lineId);
    };
}
