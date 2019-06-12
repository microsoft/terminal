/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- registry.hpp

Abstract:
- This module is used for reading/writing registry operations

Author(s):
- Michael Niksa (MiNiksa) 23-Jul-2014
- Paul Campbell (PaulCam) 23-Jul-2014

Revision History:
- From components of srvinit.c
--*/

#pragma once

#include "precomp.h"

class Registry
{
public:
    Registry(_In_ Settings* const pSettings);
    ~Registry();

    void LoadGlobalsFromRegistry();
    void LoadDefaultFromRegistry();
    void LoadFromRegistry(_In_ PCWSTR const pwszConsoleTitle);

    void GetEditKeys(_In_opt_ HKEY hConsoleKey) const;

private:
    void _LoadMappedProperties(_In_reads_(cPropertyMappings) const RegistrySerialization::RegPropertyMap* const rgPropertyMappings,
                               const size_t cPropertyMappings,
                               const HKEY hKey);

    Settings* const _pSettings;
};
