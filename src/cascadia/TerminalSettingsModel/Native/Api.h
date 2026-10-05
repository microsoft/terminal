// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#if defined(TSM_NATIVE_BUILD)
#define TSM_NATIVE_API __declspec(dllexport)
#elif defined(TSM_NATIVE_STATIC)
#define TSM_NATIVE_API
#else
#define TSM_NATIVE_API __declspec(dllimport)
#endif
