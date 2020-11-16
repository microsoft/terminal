#pragma once
// #include "IScratch.h"
#include "IScratch_h.h"
#include <wrl.h>

class __declspec(uuid("E68F5EDD-2222-4E72-A10B-4067ED8E85F2"))
    ScratchImpl : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IScratch>
{
public:
    HRESULT __stdcall MyMethod()
    {
        _count++;
        return S_OK;
    };
    HRESULT __stdcall MyCount(int* count)
    {
        *count = _count;
        return S_OK;
    };

private:
    int _count{ 0 };
};

CoCreatableClass(ScratchImpl);
