// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <vector>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class bitterator // Bit Iterator. Bitterator.
    {

    };

    class bitmap
    {
        bitmap(size_t width, size_t height) :
            _width(width),
            _height(height)
        {
            _bits.resize(width * height);
        }
        
        void set(size_t x, size_t y)
        {
            _bits[y * _width + x] = true;
        }

#ifdef UNIT_TESTING
        friend class BitmapTests;
#endif

    private:
        const size_t _width;
        const size_t _height;
        std::vector<bool> _bits;
    };
}
