/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IInputServices.hpp

Abstract:
- Defines the methods used by the console to process input.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IInputServices
    {
    public:
        virtual BOOL TranslateCharsetInfo(_Inout_ DWORD FAR* lpSrc, _Out_ LPCHARSETINFO lpCs, _In_ DWORD dwFlags) = 0;
        virtual ~IInputServices() = 0;

    protected:
        IInputServices() {}

        IInputServices(IInputServices const&) = delete;
        IInputServices& operator=(IInputServices const&) = delete;
    };

    inline IInputServices::~IInputServices() {}
}
