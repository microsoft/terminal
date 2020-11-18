// #include "pch.h" // Use stdafx.h in Visual Studio 2017 and earlier

#include "CalculatorComponent_h.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class
    __declspec(uuid("c4e46d11-dd74-43e8-a4b9-d0f789ad3751"))
        WindowBroker : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IWindowBroker>
{
public:
    WindowBroker();

    STDMETHODIMP CreateNewContent(_In_ GUID guid);
    STDMETHODIMP AddWindow(_In_ IWindowProc* pWindow);
};

CoCreatableClass(WindowBroker);
