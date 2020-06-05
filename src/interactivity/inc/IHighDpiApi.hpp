/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IHighDpiApi.hpp

Abstract:
- Defines the methods used by the console to support high DPI displays.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IHighDpiApi
    {
    public:
        virtual BOOL SetProcessDpiAwarenessContext() = 0;
        [[nodiscard]] virtual HRESULT SetProcessPerMonitorDpiAwareness() = 0;
        virtual BOOL EnablePerMonitorDialogScaling() = 0;

        virtual ~IHighDpiApi() = 0;

    protected:
        IHighDpiApi() {}

        IHighDpiApi(IHighDpiApi const&) = delete;
        IHighDpiApi& operator=(IHighDpiApi const&) = delete;
    };

    inline IHighDpiApi::~IHighDpiApi() {}
}
