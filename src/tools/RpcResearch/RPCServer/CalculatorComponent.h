// #include "pch.h" // Use stdafx.h in Visual Studio 2017 and earlier

#include "CalculatorComponent_h.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class CalculatorComponent : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ICalculatorComponent>
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
