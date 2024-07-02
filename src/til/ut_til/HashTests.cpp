// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/hash.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class HashTests
{
    TEST_CLASS(HashTests);

    TEST_METHOD(TestVectors)
    {
        struct Test
        {
            std::string_view input;
            size_t seed;
            uint64_t expected64;
            uint32_t expected32;
        };

        static constexpr std::array tests{
            Test{ "", 0, 0x93228a4de0eec5a2, 0xa45f982f },
            Test{ "a", 1, 0x0dc3b86c0704cea4, 0x09021114 },
            Test{ "abc", 2, 0x93dc6a6329f8ddba, 0xfe40215d },
            Test{ "message digest", 3, 0x0031cdc21324150f, 0x6e0fb730 },
            Test{ "abcdefghijklmnopqrstuvwxyz", 4, 0xd7523bef5b2a3fbd, 0x9435b8c2 },
            Test{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 5, 0x416757764b7bd75b, 0xccf9734c },
            Test{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890", 6, 0xa2db61f63e5a8f20, 0x9fa5ef6e },
        };

        for (const auto& t : tests)
        {
            const auto actual = til::hasher{ t.seed }.write(t.input).finalize();
#if defined(TIL_HASH_32BIT)
            VERIFY_ARE_EQUAL(t.expected32, actual);
#else
            VERIFY_ARE_EQUAL(t.expected64, actual);
#endif
        }
    }
};
