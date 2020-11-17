// #include "pch.h" // Use stdafx.h in Visual Studio 2017 and earlier

#include "CalculatorComponent_h.h"
#include <wrl.h>

using namespace Microsoft::WRL;

struct
    __declspec(uuid("c4e46d11-dd74-43e8-a4b9-d0f789ad3751"))
        WindowBroker : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IWindowBroker>
{
public:
    WindowBroker()
    {
    }

    STDMETHODIMP CreateNewContent(_In_ GUID guid)
    {
        guid;
        printf("CreateNewContent\n");
        return S_OK;
    }

    STDMETHODIMP AddWindow(_In_ IWindowProc* pWindow)
    {
        pWindow;
        printf("AddWindow\n");
        return S_OK;
    }
};

CoCreatableClass(WindowBroker);
