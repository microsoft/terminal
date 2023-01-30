// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"

class WindowThread
{
public:
    WindowThread();
    int WindowProc();

private:
    ::AppHost _host;
};
