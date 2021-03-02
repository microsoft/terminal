#pragma once

#include "ITerminalHandoff.h"

#if defined(WT_BRANDING_RELEASE)
    #define __CLSID_CTerminalHandoff "E12CFF52-A866-4C77-9A90-F570A7AA2C6B"
#elif defined(WT_BRANDING_PREVIEW)
    #define __CLSID_CTerminalHandoff "86633F1F-6454-40EC-89CE-DA4EBA977EE2"
#else
    #define __CLSID_CTerminalHandoff "051F34EE-C1FD-4B19-AF75-9BA54648434C"
#endif

using namespace Microsoft::WRL;

typedef HRESULT (*NewHandoff)(HANDLE, HANDLE, HANDLE);

struct __declspec(uuid(__CLSID_CTerminalHandoff))
    CTerminalHandoff : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ITerminalHandoff>
{
#pragma region ITerminalHandoff
    STDMETHODIMP EstablishHandoff(HANDLE in,
                                  HANDLE out,
                                  HANDLE signal);

#pragma endregion

    static HRESULT s_StartListening(NewHandoff pfnHandoff);
    static HRESULT s_StopListening();
};

CoCreatableClass(CTerminalHandoff);
