// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WexTestClass.h"
#include "../textBuffer.hpp"
#include "../../renderer/inc/DummyRenderer.hpp"
#include "../search.h"

template<>
class WEX::TestExecution::VerifyOutputTraits<std::vector<til::point_span>>
{
public:
    static WEX::Common::NoThrowString ToString(const std::vector<til::point_span>& vec)
    {
        WEX::Common::NoThrowString str;
        str.Append(L"{ ");
        for (size_t i = 0; i < vec.size(); ++i)
        {
            const auto& s = vec[i];
            if (i != 0)
            {
                str.Append(L", ");
            }
            str.AppendFormat(L"{(%d, %d), (%d, %d)}", s.start.x, s.start.y, s.end.x, s.end.y);
        }
        str.Append(L" }");
        return str;
    }
};

class UTextAdapterTests
{
    TEST_CLASS(UTextAdapterTests);

    TEST_METHOD(Unicode)
    {
        DummyRenderer renderer;
        TextBuffer buffer{ til::size{ 24, 1 }, TextAttribute{}, 0, false, &renderer };

        RowWriteState state{
            .text = L"abc 𝒶𝒷𝒸 abc ネコちゃん",
        };
        buffer.Replace(0, TextAttribute{}, state);
        VERIFY_IS_TRUE(state.text.empty());

        static constexpr auto s = [](til::CoordType beg, til::CoordType end) -> til::point_span {
            return { { beg, 0 }, { end, 0 } };
        };

        auto expected = std::vector{ s(0, 3), s(8, 11) };
        auto actual = buffer.SearchText(L"abc", SearchFlag::None);
        VERIFY_ARE_EQUAL(expected, actual);

        expected = std::vector{ s(5, 6) };
        actual = buffer.SearchText(L"𝒷", SearchFlag::None);
        VERIFY_ARE_EQUAL(expected, actual);

        expected = std::vector{ s(12, 16) };
        actual = buffer.SearchText(L"ネコ", SearchFlag::None);
        VERIFY_ARE_EQUAL(expected, actual);
    }
};
