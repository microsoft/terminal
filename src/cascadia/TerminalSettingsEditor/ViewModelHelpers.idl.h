// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define _BASE_OBSERVABLE_PROJECTED_SETTING(Type, Name) \
    Type Name                                          \
    {                                                  \
        get;                                           \
        set;                                           \
    };                                                 \
    Boolean Has##Name                                  \
    {                                                  \
        get;                                           \
    }

#define OBSERVABLE_PROJECTED_SETTING(Type, Name)    \
    _BASE_OBSERVABLE_PROJECTED_SETTING(Type, Name); \
    void Clear##Name()

#define PERMANENT_OBSERVABLE_PROJECTED_SETTING(Type, Name) \
    _BASE_OBSERVABLE_PROJECTED_SETTING(Type, Name)
