// #include "pch.h" // Use stdafx.h in Visual Studio 2017 and earlier

#include "ICalculatorComponent_h.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class
    __declspec(uuid("67fa1113-8ae3-467f-9991-7430accb45cb"))
        WindowProc : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IWindowProc>
{
public:
    WindowProc() = default;

    STDMETHODIMP GetPID(_Out_ int* pid)
    {
        *pid = GetCurrentProcessId();
        return S_OK;
    }
    STDMETHODIMP ConnectContent(_In_ GUID guid, _In_ IContentProc* content)
    {
        guid;
        content;
        return S_OK;
    }
};

CoCreatableClass(WindowProc);
