#pragma once

#include "IConsoleHandoff_h.h"

using namespace Microsoft::WRL;

struct __declspec(uuid("1F9F2BF5-5BC3-4F17-B0E6-912413F1F451"))
    Handoff : public RuntimeClass<RuntimeClassFlags<ClassicCom | InhibitFtmBase>, IConsoleHandoff>
{
#pragma region IConsoleHandoff
    STDMETHODIMP EstablishHandoff(HANDLE server,
                                  HANDLE inputEvent,
                                  PCCONSOLE_PORTABLE_ARGUMENTS args,
                                  PCCONSOLE_PORTABLE_ATTACH_MSG msg);

#pragma endregion
};

CoCreatableClass(Handoff);
