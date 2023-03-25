// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

// AtlasEngine doesn't use stb_rect_pack efficiently right now and packs rectangles one by one,
// because this simplifies the text rendering implementation quite a bit. On the flip side however,
// this allows us to skip sorting rectangles, because sorting arrays of size 1 is pointless.
#pragma warning(disable : 4505) // '...': unreferenced function with internal linkage has been removed
#define STBRP_SORT(_Base, _NumOfElements, _SizeOfElements, _CompareFunction) \
    assert(_NumOfElements == 1)

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
