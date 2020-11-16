/* file: helloc.c */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "IScratch_h.h"
#include "ICalculatorComponent_h.h"
#include <windows.h>

#include <wrl.h>
using namespace Microsoft::WRL;

const CLSID CLSID_CalculatorComponent = { 0xE68F5EDD, 0x6257, 0x4E72, 0xA1, 0x0B, 0x40, 0x67, 0xED, 0x8E, 0x85, 0xF2 };

void main()
{
    auto hr = S_OK;
    printf("Top of main()\n");
    hr = CoInitialize(nullptr);
    printf("CoInitialize -> %d\n", hr);

    ComPtr<ICalculatorComponent> calc; // Interface to COM component.
    hr = CoCreateInstance(CLSID_CalculatorComponent,
                          // auto id = __uuidof(ICalculatorComponent);
                          // id;
                          // hr = CoCreateInstance(__uuidof(ICalculatorComponent),
                          nullptr,
                          CLSCTX_LOCAL_SERVER,
                          IID_PPV_ARGS(calc.GetAddressOf()));
    if (SUCCEEDED(hr))
    {
        // Test the component by adding two numbers.
        int result;
        hr = calc->Add(4, 5, &result);

        if (FAILED(hr))
        {
            printf("Add failed: %d\n", hr);
        }
        else
        {
            printf("4+5=%d\n", result);
        }
    }
    else
    {
        // Object creation failed. Print a message.
        printf("CoCreateInstance: %d\n", hr);
    }
}
