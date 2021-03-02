#pragma once

#include "IConsoleHandoff.h"

#if defined(WT_BRANDING_RELEASE)
#define __CLSID_CConsoleHandoff "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69"
#elif defined(WT_BRANDING_PREVIEW)
#define __CLSID_CConsoleHandoff "06EC847C-C0A5-46B8-92CB-7C92F6E35CD5"
#else
#define __CLSID_CConsoleHandoff "1F9F2BF5-5BC3-4F17-B0E6-912413F1F451"
#endif

using namespace Microsoft::WRL;

struct __declspec(uuid(__CLSID_CConsoleHandoff))
    CConsoleHandoff : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IConsoleHandoff>
{
#pragma region IConsoleHandoff
    STDMETHODIMP EstablishHandoff(HANDLE server,
                                  HANDLE inputEvent,
                                  PCCONSOLE_PORTABLE_ATTACH_MSG msg);

#pragma endregion
};

CoCreatableClass(CConsoleHandoff);
