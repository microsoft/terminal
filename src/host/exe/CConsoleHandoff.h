#pragma once

#include "..\dll\IConsoleHandoff_h.h"

using namespace Microsoft::WRL;

struct __declspec(uuid(__CLSID_CConsoleHandoff))
    CConsoleHandoff : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IConsoleHandoff>
{
#pragma region IConsoleHandoff
    STDMETHODIMP EstablishHandoff(HANDLE server,
                                  HANDLE inputEvent,
                                  HANDLE in,
                                  HANDLE out,
                                  wchar_t* argString,
                                  PCCONSOLE_PORTABLE_ATTACH_MSG msg);

#pragma endregion
};

HRESULT RunAsComServer() noexcept;

CoCreatableClass(CConsoleHandoff);
