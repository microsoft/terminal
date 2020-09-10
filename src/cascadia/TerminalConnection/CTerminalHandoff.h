#pragma once

#include "..\host\dll\ITerminalHandoff_h.h"

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
