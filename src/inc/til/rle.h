// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class RunLengthEncodingTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class rle
    {
    public:
        constexpr bool operator==(const rle& other) const noexcept
        {
            return this == &other;
        }

        constexpr bool operator!=(const rle& other) const noexcept
        {
            return !(*this == other);
        }


        std::wstring to_string() const
        {
            return wil::str_printf<std::wstring>(L"I am RLE. Hear me roar.");
        }

    #ifdef UNIT_TESTING
        friend class ::RunLengthEncodingTests;
    #endif
    };
};

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::rle>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::rle& object)
        {
            return WEX::Common::NoThrowString(object.to_string().c_str());
        }
    };

    template<>
    class VerifyCompareTraits<::til::rle, ::til::rle>
    {
    public:
        static bool AreEqual(const ::til::rle& expected, const ::til::rle& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::rle& expected, const ::til::rle& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::rle& expectedLess, const ::til::rle& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::rle& expectedGreater, const ::til::rle& expectedLess) = delete;

        static bool IsNull(const ::til::rle& object) noexcept
        {
            return object == til::rle{};
        }
    };

};
#endif
