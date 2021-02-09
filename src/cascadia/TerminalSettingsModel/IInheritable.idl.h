// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define PROJECTED_SETTING(Type, Name) \
    Type Name                         \
    {                                 \
        get;                          \
        set;                          \
    };                                \
    Boolean Has##Name { get; };       \
    void Clear##Name();               \
    Microsoft.Terminal.Settings.Model.Profile Name##OverrideSource { get; }
