// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "Boundary.h"

boundary::Request boundary::Prepare(Request request, const std::wstring& directory)
{
#ifdef NATIVE_BOUNDARY
    request.directory = directory;
#else
    request.StartingDirectory(directory);
#endif
    return request;
}
