// #include "pch.h" // Use stdafx.h in Visual Studio 2017 and earlier

#include "CalculatorComponent_h.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class
    __declspec(uuid("E68F5EDD-6257-4E72-A10B-4067ED8E85F2"))
        CalculatorComponent : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ICalculatorComponent>
{
public:
    CalculatorComponent()
    {
    }

    STDMETHODIMP Add(_In_ int a, _In_ int b, _Out_ int* value)
    {
        *value = a + b;
        return S_OK;
    }
};

CoCreatableClass(CalculatorComponent);
