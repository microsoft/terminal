/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CTerminalHandoff.h

Abstract:
- This module receives an incoming request to host a terminal UX
  for a console mode application already started and attached to a PTY.

Author(s):
- Michael Niksa (MiNiksa) 31-Aug-2020

--*/

#pragma once

#include "ITerminalHandoff.h"

#if defined(WT_BRANDING_RELEASE)
#define __CLSID_CTerminalHandoff "E12CFF52-A866-4C77-9A90-F570A7AA2C6B"
#elif defined(WT_BRANDING_PREVIEW)
#define __CLSID_CTerminalHandoff "86633F1F-6454-40EC-89CE-DA4EBA977EE2"
#else
#define __CLSID_CTerminalHandoff "051F34EE-C1FD-4B19-AF75-9BA54648434C"
#endif

using NewHandoffFunction = HRESULT (*)(const HANDLE* pipes, const HANDLE* processes, const TERMINAL_STARTUP_INFO& startupInfo, PTY_HANDOFF_RESPONSE& response);

struct __declspec(uuid(__CLSID_CTerminalHandoff))
    CTerminalHandoff : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>, ITerminalHandoff3>
{
    STDMETHODIMP EstablishPtyHandoff(
        /* [size_is][system_handle][in] */ const HANDLE* pipes,
        /* [size_is][system_handle][in] */ const HANDLE* processes,
        /* [in] */ TERMINAL_STARTUP_INFO startupInfo,
        /* [out] */ PTY_HANDOFF_RESPONSE* response) noexcept override;

    static HRESULT s_StartListening(NewHandoffFunction pfnHandoff);
    static HRESULT s_StopListening();

private:
    static HRESULT s_StopListeningLocked() noexcept;
};

// Disable warnings from the CoCreatableClass macro as the value it provides for
// automatic COM class registration is of much greater value than the nits from
// the static analysis warnings.
#pragma warning(push)

#pragma warning(disable : 26477) // Macro uses 0/NULL over nullptr.
#pragma warning(disable : 26476) // Macro uses naked union over variant.

CoCreatableClass(CTerminalHandoff);

#pragma warning(pop)
