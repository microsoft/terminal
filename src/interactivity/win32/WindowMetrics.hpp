/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WindowMetrics.hpp

Abstract:
- Win32 implementation of the IWindowMetrics interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#include "..\inc\IWindowMetrics.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowMetrics final : public IWindowMetrics
    {
    public:
        // IWindowMetrics Members
        ~WindowMetrics() = default;
        RECT GetMinClientRectInPixels();
        RECT GetMaxClientRectInPixels();

        // Public Members
        RECT GetMaxWindowRectInPixels();
        RECT GetMaxWindowRectInPixels(const RECT* const prcSuggested, _Out_opt_ UINT* pDpiSuggested);

        BOOL AdjustWindowRectEx(_Inout_ LPRECT prc,
                                const DWORD dwStyle,
                                const BOOL fMenu,
                                const DWORD dwExStyle);
        BOOL AdjustWindowRectEx(_Inout_ LPRECT prc,
                                const DWORD dwStyle,
                                const BOOL fMenu,
                                const DWORD dwExStyle,
                                const int iDpi);

        void ConvertClientRectToWindowRect(_Inout_ RECT* const prc);
        void ConvertWindowRectToClientRect(_Inout_ RECT* const prc);

    private:
        enum ConvertRectangle
        {
            CLIENT_TO_WINDOW,
            WINDOW_TO_CLIENT
        };

        BOOL UnadjustWindowRectEx(_Inout_ LPRECT prc,
                                  const DWORD dwStyle,
                                  const BOOL fMenu,
                                  const DWORD dwExStyle);

        void ConvertRect(_Inout_ RECT* const prc, const ConvertRectangle crDirection);
    };
}
