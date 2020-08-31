/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DelegationConfig.hpp

Abstract:
- This module is used for looking up delegation handlers for the launch of the defualt console hosting environment

Author(s):
- Michael Niksa (MiNiksa) 31-Aug-2020

--*/

#pragma once

class DelegationConfig
{
public:
    [[nodiscard]] static HRESULT s_Get(IID& iid);
};
