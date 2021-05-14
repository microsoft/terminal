// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/rle.h"
#include "consoletaeftemplates.hpp"

using namespace std::literals;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace WEX::TestExecution
{
    template<typename T, typename S, typename Container>
    class VerifyCompareTraits<::std::string_view, ::til::basic_rle<T, S, Container>>
    {
        using rle_vector = ::til::basic_rle<T, S, Container>;
        using value_type = typename rle_vector::value_type;

    public:
        static bool AreEqual(const ::std::string_view& expected, const rle_vector& actual) noexcept
        {
            auto it = expected.data();
            const auto end = it + expected.size();
            size_t expected_size = 0;

            for (const auto& run : actual.runs())
            {
                const auto actual_value = run.value;
                const auto length = run.length;

                if (length == 0)
                {
                    return false;
                }

                for (size_t i = 0; i < length; ++it)
                {
                    if (it == end)
                    {
                        return false;
                    }

                    const auto ch = *it;

                    if (ch == '|' && i != 0)
                    {
                        return false;
                    }

                    if (ch == ' ' && i == 0)
                    {
                        return false;
                    }

                    if (ch < '0' || ch > '9')
                    {
                        continue;
                    }

                    const value_type expected_value = ch - '0';
                    if (expected_value != actual_value)
                    {
                        return false;
                    }

                    ++i;
                    ++expected_size;
                }
            }

            return it == end && expected_size == actual.size();
        }

        static bool AreSame(const ::std::string_view& expected, const rle_vector& actual) noexcept
        {
            return false;
        }

        static bool IsLessThan(const ::std::string_view& expectedLess, const rle_vector& expectedGreater) = delete;
        static bool IsGreaterThan(const ::std::string_view& expectedGreater, const rle_vector& expectedLess) = delete;
        static bool IsNull(const ::std::string_view& object) = delete;
    };
}

class RunLengthEncodingTests
{
    using rle_vector = til::small_rle<uint16_t, uint16_t, 16>;
    using value_type = rle_vector::value_type;
    using size_type = rle_vector::size_type;
    using rle_type = rle_vector::rle_type;

    using basic_container = std::basic_string<value_type>;
    using basic_container_view = std::basic_string_view<value_type>;
    using rle_container = rle_vector::container;

    static rle_container rle_encode(const std::string_view& from)
    {
        if (from.empty())
        {
            return {};
        }

        rle_container to;
        value_type value = from.front() - '0';
        size_type length = 0;

        for (auto ch : from)
        {
            if (ch < '0' || ch > '9')
            {
                continue;
            }

            const value_type val = ch - '0';
            if (val != value)
            {
                to.emplace_back(value, length);
                value = val;
                length = 0;
            }

            length++;
        }

        if (length)
        {
            to.emplace_back(value, length);
        }

        return to;
    }

    static rle_container rle_encode(const basic_container_view& from)
    {
        if (from.empty())
        {
            return {};
        }

        rle_container to;
        value_type value = from.front();
        size_type length = 0;

        for (auto v : from)
        {
            if (v != value)
            {
                to.emplace_back(value, length);
                value = v;
                length = 0;
            }

            length++;
        }

        if (length)
        {
            to.emplace_back(value, length);
        }

        return to;
    }

    static basic_container rle_decode(const rle_container& from)
    {
        basic_container to;
        to.reserve(from.size());

        for (const auto& run : from)
        {
            for (size_t i = 0; i < run.length; ++i)
            {
                to.push_back(run.value);
            }
        }

        return to;
    }

    TEST_CLASS(RunLengthEncodingTests)

    TEST_METHOD(ConstructDefault)
    {
        rle_vector rle{};
        VERIFY_ARE_EQUAL(0u, rle.size());
        VERIFY_IS_TRUE(rle.empty());

        // We're testing replace() elsewhere, but this is special:
        // This ensures that even if we're default constructed we can add data.
        rle.replace(0, 0, { 1, 5 });
        VERIFY_ARE_EQUAL(5u, rle.size());
        VERIFY_IS_FALSE(rle.empty());
    }

    TEST_METHOD(ConstructWithInitializerList)
    {
        rle_vector rle{ { { 1, 3 }, { 2, 2 }, { 1, 3 } } };
        VERIFY_ARE_EQUAL("1 1 1|2 2|1 1 1"sv, rle);
    }

    TEST_METHOD(ConstructWithLengthAndValue)
    {
        rle_vector rle(5, 1);
        VERIFY_ARE_EQUAL("1 1 1 1 1"sv, rle);
    }

    TEST_METHOD(CopyAndMove)
    {
        constexpr auto expected_full = "1 1 1|2 2|1 1 1"sv;
        constexpr auto expected_empty = ""sv;

        rle_vector rle1{ { { 1, 3 }, { 2, 2 }, { 1, 3 } } };
        rle_vector rle2;
        VERIFY_ARE_EQUAL(expected_full, rle1);
        VERIFY_ARE_EQUAL(expected_empty, rle2);

        // swap
        rle1.swap(rle2);
        VERIFY_ARE_EQUAL(expected_empty, rle1);
        VERIFY_ARE_EQUAL(expected_full, rle2);

        // copy
        rle1 = rle2;
        VERIFY_ARE_EQUAL(expected_full, rle1);
        VERIFY_ARE_EQUAL(expected_full, rle2);

        // prepare rle1 for the upcoming move

        // move
        rle1 = { { { 1, 1 } } };
        rle1 = std::move(rle2);
        VERIFY_ARE_EQUAL(expected_full, rle1);
    }

    TEST_METHOD(At)
    {
        rle_vector rle{
            {
                { 1, 1 },
                { 3, 2 },
                { 2, 1 },
                { 1, 3 },
                { 5, 2 },
            }
        };

        VERIFY_ARE_EQUAL(1u, rle.at(0));
        VERIFY_ARE_EQUAL(3u, rle.at(1));
        VERIFY_ARE_EQUAL(3u, rle.at(2));
        VERIFY_ARE_EQUAL(2u, rle.at(3));
        VERIFY_ARE_EQUAL(1u, rle.at(4));
        VERIFY_ARE_EQUAL(1u, rle.at(5));
        VERIFY_ARE_EQUAL(1u, rle.at(6));
        VERIFY_ARE_EQUAL(5u, rle.at(7));
        VERIFY_ARE_EQUAL(5u, rle.at(8));
        VERIFY_THROWS(rle.at(9), std::out_of_range);
    }

    TEST_METHOD(Slice)
    {
        rle_vector rle{
            {
                { 1, 1 },
                { 3, 2 },
                { 2, 1 },
                { 1, 3 },
                { 5, 2 },
            }
        };

        VERIFY_ARE_EQUAL("1|3 3|2|1 1 1|5 5"sv, rle);
        // empty
        VERIFY_ARE_EQUAL(""sv, rle.slice(0, 0)); // begin
        VERIFY_ARE_EQUAL(""sv, rle.slice(1, 1)); // between two runs
        VERIFY_ARE_EQUAL(""sv, rle.slice(2, 2)); // within a run
        VERIFY_ARE_EQUAL(""sv, rle.slice(rle.size(), rle.size())); // end
        VERIFY_ARE_EQUAL(""sv, rle.slice(5, 0)); // end_index > begin_index
        VERIFY_ARE_EQUAL(""sv, rle.slice(1000, 900)); // end_index > begin_index
        // full copy
        VERIFY_ARE_EQUAL("1|3 3|2|1 1 1|5 5"sv, rle.slice(0, rle.size()));
        // between two runs -> between two runs
        VERIFY_ARE_EQUAL("1|3 3|2|1 1 1"sv, rle.slice(0, 7));
        VERIFY_ARE_EQUAL("2|1 1 1"sv, rle.slice(3, 7));
        // between two runs -> within a run
        VERIFY_ARE_EQUAL("3 3|2|1"sv, rle.slice(1, 5));
        VERIFY_ARE_EQUAL("3 3|2|1 1"sv, rle.slice(1, 6));
        // within a run -> between two runs
        VERIFY_ARE_EQUAL("3|2|1 1 1|5 5"sv, rle.slice(2, rle.size()));
        VERIFY_ARE_EQUAL("3|2|1 1 1"sv, rle.slice(2, 7));
        // within a run -> within a run
        VERIFY_ARE_EQUAL("3|2|1"sv, rle.slice(2, 5));
        VERIFY_ARE_EQUAL("3|2|1 1"sv, rle.slice(2, 6));
    }

    TEST_METHOD(Replace)
    {
        struct TestCase
        {
            std::string_view source;

            size_type start_index;
            size_type end_index;
            std::string_view change;

            std::string_view expected;
        };

        std::array<TestCase, 29> test_cases{
            {
                // empty source
                { "", 0, 0, "", "" },
                { "", 0, 0, "1|2|3", "1|2|3" },

                // empty change
                { "1|2|3", 0, 0, "", "1|2|3" },
                { "1|2|3", 2, 2, "", "1|2|3" },
                { "1|2|3", 3, 3, "", "1|2|3" },

                // remove
                { "1|3 3|2|1 1 1|5 5", 0, 9, "", "" }, // all
                { "1|3 3|2|1 1 1|5 5", 0, 6, "", "1|5 5" }, // beginning
                { "1|3 3|2|1 1 1|5 5", 6, 9, "", "1|3 3|2|1 1" }, // end
                { "1|3 3|2|1 1 1|5 5", 3, 7, "", "1|3 3|5 5" }, // middle, between runs
                { "1|3 3|2|1 1 1|5 5", 2, 6, "", "1|3|1|5 5" }, // middle, within runs

                // insert
                { "1|3 3|2|1 1 1|5 5", 0, 0, "6|7 7|8", "6|7 7|8|1|3 3|2|1 1 1|5 5" }, // beginning
                { "1|3 3|2|1 1 1|5 5", 9, 9, "6|7 7|8", "1|3 3|2|1 1 1|5 5|6|7 7|8" }, // end
                { "1|3 3|2|1 1 1|5 5", 4, 4, "6|7 7|8", "1|3 3|2|6|7 7|8|1 1 1|5 5" }, // middle, between runs
                { "1|3 3|2|1 1 1|5 5", 5, 5, "6|7 7|8", "1|3 3|2|1|6|7 7|8|1 1|5 5" }, // middle, within runs
                { "1|3 3|2|1 1 1|5 5", 6, 6, "6", "1|3 3|2|1 1|6|1|5 5" }, // middle, within runs, single run

                // replace
                { "1|3 3|2|1 1 1|5 5", 0, 9, "6|7 7|8", "6|7 7|8" }, // all
                { "1|3 3|2|1 1 1|5 5", 0, 6, "6|7 7|8", "6|7 7|8|1|5 5" }, // beginning
                { "1|3 3|2|1 1 1|5 5", 6, 9, "6|7 7|8", "1|3 3|2|1 1|6|7 7|8" }, // end
                { "1|3 3|2|1 1 1|5 5", 3, 7, "6|7 7|8", "1|3 3|6|7 7|8|5 5" }, // middle, between runs
                { "1|3 3|2|1 1 1|5 5", 3, 7, "6|7 7 7", "1|3 3|6|7 7 7|5 5" }, // middle, between runs, same size
                { "1|3 3|2|1 1 1|5 5", 2, 6, "6|7 7|8", "1|3|6|7 7|8|1|5 5" }, // middle, within runs
                { "1|3 3|2|1 1 1|5 5", 2, 6, "6", "1|3|6|1|5 5" }, // middle, within runs, single run

                // join with predecessor/successor run
                { "1|3 3|2|1 1 1|5 5", 0, 3, "1|2 2", "1|2 2 2|1 1 1|5 5" }, // beginning
                { "1|3 3|2|1 1 1|5 5", 7, 9, "1|5", "1|3 3|2|1 1 1 1|5" }, // end
                { "1|3 3|2|1 1 1|5 5", 1, 4, "1|2|1", "1 1|2|1 1 1 1|5 5" }, // middle, between runs
                { "1|3 3|2|1 1 1|5 5", 2, 6, "3 3|1", "1|3 3 3|1 1|5 5" }, // middle, within runs
                { "1|3 3|2|1 1 1|5 5", 1, 6, "1", "1 1 1|5 5" }, // middle, within runs, single run
                { "1|3 3|2|1 1 1|5 5", 1, 4, "", "1 1 1 1|5 5" }, // middle, within runs, no runs
            }
        };

        int idx = 0;

        for (const auto& test_case : test_cases)
        {
            rle_vector rle{ rle_encode(test_case.source) };
            const auto change = rle_encode(test_case.change);

            rle.replace(test_case.start_index, test_case.end_index, change);

            try
            {
                VERIFY_ARE_EQUAL(test_case.expected, rle);
            }
            catch (...)
            {
                // I couldn't figure out how to attach additional info
                // to a failed assertion so I'm doing it this way...
                Log::Comment(NoThrowString().Format(
                    L"test case:   %d\nsource:      %hs\nstart_index: %u\nend_index:   %u\nchange:      %hs\nexpected:    %hs\nactual:      %s",
                    idx,
                    test_case.source.data(),
                    test_case.start_index,
                    test_case.end_index,
                    test_case.change.data(),
                    test_case.expected.data(),
                    rle.to_string().c_str()));
                throw;
            }

            ++idx;
        }
    }

    TEST_METHOD(ReplaceValues)
    {
        struct TestCase
        {
            std::string_view source;

            value_type old_value;
            value_type new_value;

            std::string_view expected;
        };

        std::array<TestCase, 6> test_cases{
            {
                // empty source
                { "", 1, 2, "" },
                // no changes
                { "3|4|5", 1, 2, "3|4|5" },
                // begin
                { "1 1|2|3|4", 1, 2, "2 2 2|3|4" },
                // end
                { "4|3|2|1 1", 1, 2, "4|3|2 2 2" },
                // middle
                { "3|2|1|2|4", 1, 2, "3|2 2 2|4" },
                // middle
                { "3|1|2|1|4", 1, 2, "3|2 2 2|4" },
            }
        };

        int idx = 0;

        for (const auto& test_case : test_cases)
        {
            rle_vector rle{ rle_encode(test_case.source) };

            rle.replace_values(test_case.old_value, test_case.new_value);

            try
            {
                VERIFY_ARE_EQUAL(test_case.expected, rle);
            }
            catch (...)
            {
                // I couldn't figure out how to attach additional info
                // to a failed assertion so I'm doing it this way...
                Log::Comment(NoThrowString().Format(
                    L"test case: %d\nsource:    %hs\nold_value: %u\nnew_value: %u\nexpected:  %hs\nactual:    %s",
                    idx,
                    test_case.source.data(),
                    test_case.old_value,
                    test_case.new_value,
                    test_case.expected.data(),
                    rle.to_string().c_str()));
                throw;
            }

            ++idx;
        }
    }

    TEST_METHOD(ResizeTrailingExtent)
    {
        constexpr std::string_view data{ "133211155" };

        for (size_type length = 0; length <= data.size(); length++)
        {
            rle_vector rle{ rle_encode(data) };
            rle.resize_trailing_extent(length);
            VERIFY_ARE_EQUAL(data.substr(0, length), rle);
        }
    }

    TEST_METHOD(Comparison)
    {
        rle_vector rle1{ { { 1, 1 }, { 3, 2 }, { 2, 1 } } };
        rle_vector rle2{ rle1 };

        VERIFY_IS_TRUE(rle1 == rle2);
        VERIFY_IS_FALSE(rle1 != rle2);

        rle2.replace(0, 1, 2);

        VERIFY_IS_FALSE(rle1 == rle2);
        VERIFY_IS_TRUE(rle1 != rle2);
    }

    TEST_METHOD(Iterators)
    {
        constexpr std::string_view expected{ "133211155" };
        rle_vector rle{ rle_encode(expected) };

        {
            std::string actual;
            actual.reserve(expected.size());

            for (auto v : rle)
            {
                actual.push_back(static_cast<char>(v + '0'));
            }

            VERIFY_ARE_EQUAL(expected, actual);
        }

        {
            auto it = rle.begin();
            const auto end = rle.end();

            it += 2;
            VERIFY_ARE_EQUAL(3u, *it);

            it += 1;
            VERIFY_ARE_EQUAL(2u, *it);

            it += 3;
            VERIFY_ARE_EQUAL(1u, *it);

            it += 2;
            VERIFY_ARE_EQUAL(5u, *it);

            ++it;
            VERIFY_ARE_EQUAL(end, it);
        }
    }
};
