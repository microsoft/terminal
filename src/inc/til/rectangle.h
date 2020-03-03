// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "point.h"
#include "size.h"

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class rectangle
    {

#ifdef UNIT_TESTING
        friend class RectangleTests;
#endif
    protected:
        til::point _topLeft;
        til::size _size;
    };
}
