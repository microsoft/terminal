// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// We're suspending the inclusion of til here so that we can include
// it after some of our C++/WinRT headers.
#define BLOCK_TIL
#include <LibraryIncludes.h>
#include "winrt/Windows.Foundation.h"

#include "winrt/Microsoft.Terminal.Core.h"
#include <til.h>
