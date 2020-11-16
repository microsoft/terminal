/* file: hellos.c */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include "ScratchImpl.h"
#include "CalculatorComponent.h"
#include <windows.h>
#include <memory>
#include <conio.h>

using namespace Microsoft::WRL;

void main()
{
    auto hr = S_OK;
    printf("Top of main()\n");
    hr = CoInitialize(nullptr);
    printf("CoInitialize -> %d\n", hr);

    // create a module object using WRL::Module<OutOfProc>::Create(),
    // and call module.RegisterObjects() on startup,
    // and module.UnregisterObjects()
    // and module.Terminate() on shutdown.

    // create a module object using WRL::Module<OutOfProc>::Create(),
    auto& mod{ Module<OutOfProc>::Create([] {
        printf("Create callback\n");
        auto hr = S_OK;
        auto& modInner = Module<OutOfProc>::GetModule();
        // and module.UnregisterObjects()
        hr = modInner.UnregisterObjects();
        printf("UnregisterObjects -> %d\n", hr);

        // and module.Terminate() on shutdown.
        hr = modInner.Terminate();
        printf("Terminate -> %d\n", hr);

        printf("type a character to exit\n");
        auto ch = _getch();
        ch;
        printf("getch\n");
    }) };
    // and call module.RegisterObjects() on startup,
    hr = mod.RegisterObjects();
    printf("RegisterObjects -> %d\n", hr);

    // printf("type a character to exit\n");
    // auto ch = _getch();
    // ch;
    // printf("getch\n");

    // // and module.UnregisterObjects()
    // hr = module.UnregisterObjects();
    // printf("UnregisterObjects -> %d\n", hr);

    // // and module.Terminate() on shutdown.
    // hr = module.Terminate();
    // printf("Terminate -> %d\n", hr);

    SetProcessShutdownParameters(0, 0);

    ExitThread(hr);
}
