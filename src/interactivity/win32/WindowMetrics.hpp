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

#include "../inc/IWindowMetrics.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowMetrics final : public IWindowMetrics
    {
    public:
        // IWindowMetrics Members
        ~WindowMetrics() = default;
        til::rect GetMinClientRectInPixels();
        til::rect GetMaxClientRectInPixels();

        // Public Members
        til::rect GetMaxWindowRectInPixels();
        til::rect GetMaxWindowRectInPixels(const til::rect* const prcSuggested, _Out_opt_ UINT* pDpiSuggested);

        BOOL AdjustWindowRectEx(_Inout_ til::rect* prc,
                                const DWORD dwStyle,
                                const BOOL fMenu,
                                const DWORD dwExStyle);
        BOOL AdjustWindowRectEx(_Inout_ til::rect* prc,
                                const DWORD dwStyle,
                                const BOOL fMenu,
                                const DWORD dwExStyle,
                                const int iDpi);

        void ConvertClientRectToWindowRect(_Inout_ til::rect* const prc);
        void ConvertWindowRectToClientRect(_Inout_ til::rect* const prc);

    private:
        enum ConvertRectangle
        {
            CLIENT_TO_WINDOW,
            WINDOW_TO_CLIENT
        };

        BOOL UnadjustWindowRectEx(_Inout_ til::rect* prc,
                                  const DWORD dwStyle,
                                  const BOOL fMenu,
                                  const DWORD dwExStyle);

        void ConvertRect(_Inout_ til::rect* const prc, const ConvertRectangle crDirection);
    };
}
