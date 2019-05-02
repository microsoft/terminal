// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include "util.h"

// This application exists to be connected to a console session while doing exactly nothing.
// This keeps a console session alive and doesn't interfere with tests or other hooks.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    TestLibFunc();

    return 0;
}
