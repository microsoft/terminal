// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "Boundary.h"

boundary::Request boundary::Build(const LaunchValues& input,
                                 const std::wstring& title,
                                 const std::wstring& directory,
                                 const std::wstring& modelDll)
{
    return Prepare(Edit(Load(input, modelDll), title), directory);
}
