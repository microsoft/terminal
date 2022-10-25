// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ColorTests
{
    TEST_CLASS(ColorTests);

    TEST_METHOD(Construct)
    {
        til::color rgb{ 0xde, 0xad, 0xbe };

        VERIFY_ARE_EQUAL(0xde, rgb.r);
        VERIFY_ARE_EQUAL(0xad, rgb.g);
        VERIFY_ARE_EQUAL(0xbe, rgb.b);
        VERIFY_ARE_EQUAL(0xff, rgb.a); // auto-filled by constructor

        VERIFY_ARE_EQUAL(rgb, rgb);

        til::color rgba{ 0xde, 0xad, 0xbe, 0xef };

        VERIFY_ARE_EQUAL(0xde, rgba.r);
        VERIFY_ARE_EQUAL(0xad, rgba.g);
        VERIFY_ARE_EQUAL(0xbe, rgba.b);
        VERIFY_ARE_EQUAL(0xef, rgba.a);

        VERIFY_ARE_NOT_EQUAL(rgb, rgba);
    }

    TEST_METHOD(ConvertFromColorRef)
    {
        COLORREF c = 0x00FEEDFAu; // remember, this one is in 0BGR
        til::color fromColorRef{ c };

        VERIFY_ARE_EQUAL(0xfa, fromColorRef.r);
        VERIFY_ARE_EQUAL(0xed, fromColorRef.g);
        VERIFY_ARE_EQUAL(0xfe, fromColorRef.b);
        VERIFY_ARE_EQUAL(0xff, fromColorRef.a); // COLORREF do not have an alpha channel
    }

    TEST_METHOD(ConvertToColorRef)
    {
        til::color rgb{ 0xf0, 0x0d, 0xca, 0xfe };

        VERIFY_ARE_EQUAL(0x00CA0DF0u, static_cast<COLORREF>(rgb)); // alpha is dropped, COLORREF is 0BGR
    }

    template<typename T>
    struct Quad_rgba
    {
        T r, g, b, a;
    };

    template<typename T>
    struct Quad_RGBA
    {
        T R, G, B, A;
    };

    TEST_METHOD(ConvertFromIntColorStructs)
    {
        Quad_rgba<int> q1{ 0xca, 0xfe, 0xf0, 0x0d };
        til::color t1{ 0xca, 0xfe, 0xf0, 0x0d };

        VERIFY_ARE_EQUAL(t1, static_cast<til::color>(q1));

        Quad_RGBA<int> q2{ 0xfa, 0xce, 0xb0, 0x17 };
        til::color t2{ 0xfa, 0xce, 0xb0, 0x17 };

        VERIFY_ARE_EQUAL(t2, static_cast<til::color>(q2));
    }

    TEST_METHOD(ConvertFromFloatColorStructs)
    {
        Quad_rgba<float> q1{ 0.730f, 0.867f, 0.793f, 0.997f };
        til::color t1{ 0xba, 0xdd, 0xca, 0xfe };

        VERIFY_ARE_EQUAL(t1, static_cast<til::color>(q1));

        Quad_RGBA<float> q2{ 0.871f, 0.679f, 0.981f, 0.067f };
        til::color t2{ 0xde, 0xad, 0xfa, 0x11 };

        VERIFY_ARE_EQUAL(t2, static_cast<til::color>(q2));
    }

    TEST_METHOD(WithAlpha)
    {
        const COLORREF c = 0x00FEEDFAu; // remember, this one is in 0BGR
        const til::color fromColorRef{ c };

        VERIFY_ARE_EQUAL(0xfa, fromColorRef.r);
        VERIFY_ARE_EQUAL(0xed, fromColorRef.g);
        VERIFY_ARE_EQUAL(0xfe, fromColorRef.b);
        VERIFY_ARE_EQUAL(0xff, fromColorRef.a); // COLORREF do not have an alpha channel

        const auto colorWithAlpha{ fromColorRef.with_alpha(0x7f) };

        VERIFY_ARE_NOT_EQUAL(colorWithAlpha, fromColorRef);

        VERIFY_ARE_EQUAL(0xfa, colorWithAlpha.r);
        VERIFY_ARE_EQUAL(0xed, colorWithAlpha.g);
        VERIFY_ARE_EQUAL(0xfe, colorWithAlpha.b);
        VERIFY_ARE_EQUAL(0x7f, colorWithAlpha.a);
    }

    TEST_METHOD(LayerOver)
    {
        static constexpr til::color orange{ 255, 165, 0, 255 };
        static constexpr til::color blue{ 0, 205, 255, 255 };
        static constexpr til::color orangeWithAlpha{ 255, 165, 0, 165 };
        static constexpr til::color blueWithAlpha{ 0, 205, 255, 205 };

        VERIFY_ARE_EQUAL(orange, orange.layer_over(blue));
        VERIFY_ARE_EQUAL(blue, blue.layer_over(orange));
        VERIFY_ARE_EQUAL(til::color(165, 179, 90, 255), orangeWithAlpha.layer_over(blue));
        VERIFY_ARE_EQUAL(til::color(177, 177, 78, 237), orangeWithAlpha.layer_over(blueWithAlpha));
        VERIFY_ARE_EQUAL(til::color(50, 197, 205, 255), blueWithAlpha.layer_over(orange));
        VERIFY_ARE_EQUAL(til::color(35, 200, 220, 237), blueWithAlpha.layer_over(orangeWithAlpha));
    }
};
