// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define _BASE_INHERITABLE_SETTING(Type, Name) \
    Type Name                                 \
    {                                         \
        get;                                  \
        set;                                  \
    };                                        \
    Boolean Has##Name                         \
    {                                         \
        get;                                  \
    };                                        \
    void Clear##Name()

#define INHERITABLE_SETTING(Type, Name) \
    _BASE_INHERITABLE_SETTING(Type, Name)
