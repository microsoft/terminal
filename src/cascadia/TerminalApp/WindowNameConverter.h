// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "WindowIdConverter.g.h"
#include "WindowNameConverter.g.h"
#include "../inc/cppwinrt_utils.h"

DECLARE_CONVERTER(winrt::TerminalApp, WindowIdConverter);
DECLARE_CONVERTER(winrt::TerminalApp, WindowNameConverter);
