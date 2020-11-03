#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

void printPeasants(const winrt::MonarchPeasantSample::Monarch& /*monarch*/)
{
    printf("This is unimplemented\n");
}

bool monarchAppLoop(AppState& state)
{
    bool exitRequested = false;
    printf("Press `l` to list peasants, `q` to quit\n");

    while (!exitRequested)
    {
        const auto ch = _getch();
        if (ch == 'l')
        {
            printPeasants(state._monarch);
        }
        else if (ch == 'q')
        {
            exitRequested = true;
        }
    }
    return true;
}
