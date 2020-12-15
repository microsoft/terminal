// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define OBSERVABLE_PROJECTED_SETTING(Type, Name) \
    Type Name                                    \
    {                                            \
        get;                                     \
        set;                                     \
    };                                           \
    Boolean Has##Name { get; };                  \
    void Clear##Name()
