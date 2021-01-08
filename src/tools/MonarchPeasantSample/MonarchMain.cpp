#include "pch.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

void printPeasants(const winrt::MonarchPeasantSample::Monarch& /*monarch*/)
{
    printf("This is unimplemented\n");
}

bool monarchAppLoop(AppState& state)
{
    bool exitRequested = false;
    printf("Press `l` to list peasants, 'm' to change modes `q` to quit\n");

    winrt::com_ptr<winrt::MonarchPeasantSample::implementation::Monarch> monarchImpl;
    monarchImpl.copy_from(winrt::get_self<winrt::MonarchPeasantSample::implementation::Monarch>(state.monarch));

    while (!exitRequested)
    {
        const auto ch = _getch();
        if (ch == 'l')
        {
            printPeasants(state.monarch);
        }
        else if (ch == 'q')
        {
            exitRequested = true;
        }
        else if (ch == 'm')
        {
            if (monarchImpl)
            {
                monarchImpl->ToggleWindowingBehavior();
            }
        }
    }
    return true;
}
