/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Module.hpp

Abstract:
- Lists all the interfaces for which there exist multiple implementations that
  can be picked amongst depending on API's available on the host OS.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    enum class Module
    {
        AccessibilityNotifier,
        ConsoleControl,
        ConsoleInputThread,
        ConsoleWindowMetrics,
        HighDpiApi,
        InputServices,
        SystemConfigurationProvider
    };
}
