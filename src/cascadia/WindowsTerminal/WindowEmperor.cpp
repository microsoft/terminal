// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowEmperor.h"
#include "WindowThread.h"

WindowEmperor::WindowEmperor() noexcept :
    _app{}
{
}

bool WindowEmperor::ShouldExit()
{
    // TODO!
    return false;
}

void WindowEmperor::WaitForWindows()
{
    std::thread one{ [this]() {
        WindowThread foo{ _app.Logic() };
        return foo.WindowProc();
    } };

    Sleep(2000);

    std::thread two{ [this]() {
        WindowThread foo{ _app.Logic() };
        return foo.WindowProc();
    } };

    one.join();
    two.join();
}
