#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

void managerApp()
{
}

int main(int argc, char** argv)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    init_apartment();

    try
    {
        managerApp();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("Exiting client");
}
