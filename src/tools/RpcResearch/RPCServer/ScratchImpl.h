#pragma once
// #include "IScratch.h"
#include "IScratch_h.h"
#include <wrl.h>

class ScratchImpl : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IScratch>
{
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

// CoCreatableClass(ScratchImpl);
