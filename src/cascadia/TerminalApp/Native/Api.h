// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#if defined(APP_NATIVE_BUILD)
#define APP_NATIVE_API __declspec(dllexport)
#elif defined(APP_NATIVE_STATIC)
#define APP_NATIVE_API
#else
#define APP_NATIVE_API __declspec(dllimport)
#endif
