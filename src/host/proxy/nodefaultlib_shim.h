// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <guiddef.h>

#define memcmp(a, b, c) (!InlineIsEqualGUID(a, b))
