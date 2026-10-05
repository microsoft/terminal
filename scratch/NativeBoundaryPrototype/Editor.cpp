// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "Boundary.h"

boundary::Request boundary::Edit(Request request, const std::wstring& title)
{
#ifdef NATIVE_BOUNDARY
    request.title = title;
#else
    request.TabTitle(title);
#endif
    return request;
}
