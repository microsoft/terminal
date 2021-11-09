// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Logging;

class EnumSetTests
{
    TEST_CLASS(EnumSetTests);

    TEST_METHOD(Constructors)
    {
        enum class Flags
        {
            Zero,
            One,
            Two,
            Three,
            Four
        };

        {
            Log::Comment(L"Default constructor with no bits set");
            til::enumset<Flags> flags;
            VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());
        }

        {
            Log::Comment(L"Constructor with bit 3 set");
            til::enumset<Flags> flags{ Flags::Three };
            VERIFY_ARE_EQUAL(0b01000u, flags.to_ulong());
        }

        {
            Log::Comment(L"Constructor with bits 0, 2, and 4 set");
            til::enumset<Flags> flags{ Flags::Zero, Flags::Two, Flags::Four };
            VERIFY_ARE_EQUAL(0b10101u, flags.to_ulong());
        }
    }

    TEST_METHOD(SetResetFlipMethods)
    {
        enum class Flags
        {
            Zero,
            One,
            Two,
            Three,
            Four
        };

        Log::Comment(L"Start with no bits set");
        til::enumset<Flags> flags;
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());

        Log::Comment(L"Set bit 2 to true");
        flags.set(Flags::Two);
        VERIFY_ARE_EQUAL(0b00100u, flags.to_ulong());

        Log::Comment(L"Flip bit 4 to true");
        flags.flip(Flags::Four);
        VERIFY_ARE_EQUAL(0b10100u, flags.to_ulong());

        Log::Comment(L"Set bit 0 to true");
        flags.set(Flags::Zero);
        VERIFY_ARE_EQUAL(0b10101u, flags.to_ulong());

        Log::Comment(L"Reset bit 2 to false, leaving 0 and 4 true");
        flags.reset(Flags::Two);
        VERIFY_ARE_EQUAL(0b10001u, flags.to_ulong());

        Log::Comment(L"Set bit 0 to false, leaving 4 true");
        flags.set(Flags::Zero, false);
        VERIFY_ARE_EQUAL(0b10000u, flags.to_ulong());

        Log::Comment(L"Flip bit 4, leaving all bits false ");
        flags.flip(Flags::Four);
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());

        Log::Comment(L"Set bits 0, 3, and 2");
        flags.set_all(Flags::Zero, Flags::Three, Flags::Two);
        VERIFY_ARE_EQUAL(0b01101u, flags.to_ulong());

        Log::Comment(L"Reset bits 3, 4 (already reset), and 0, leaving 2 true");
        flags.reset_all(Flags::Three, Flags::Four, Flags::Zero);
        VERIFY_ARE_EQUAL(0b00100u, flags.to_ulong());
    }

    TEST_METHOD(TestMethods)
    {
        enum class Flags
        {
            Zero,
            One,
            Two,
            Three,
            Four
        };

        Log::Comment(L"Start with bits 0, 2, and 4 set");
        til::enumset<Flags> flags{ Flags::Zero, Flags::Two, Flags::Four };
        VERIFY_ARE_EQUAL(0b10101u, flags.to_ulong());

        Log::Comment(L"Test bits 1 and 2 with the test method");
        VERIFY_IS_FALSE(flags.test(Flags::One));
        VERIFY_IS_TRUE(flags.test(Flags::Two));

        Log::Comment(L"Test bit 3 and 4 with the array operator");
        VERIFY_IS_FALSE(flags[Flags::Three]);
        VERIFY_IS_TRUE(flags[Flags::Four]);

        Log::Comment(L"Test if any bits are set");
        VERIFY_IS_TRUE(flags.any());
        Log::Comment(L"Test if either bit 1 or 3 are set");
        VERIFY_IS_FALSE(flags.any(Flags::One, Flags::Three));
        Log::Comment(L"Test if either bit 1 or 4 are set");
        VERIFY_IS_TRUE(flags.any(Flags::One, Flags::Four));

        Log::Comment(L"Test if all bits are set");
        VERIFY_IS_FALSE(flags.all());
        Log::Comment(L"Test if both bits 0 and 4 are set");
        VERIFY_IS_TRUE(flags.all(Flags::Zero, Flags::Four));
        Log::Comment(L"Test if both bits 0 and 3 are set");
        VERIFY_IS_FALSE(flags.all(Flags::Zero, Flags::Three));
    }

    TEST_METHOD(ArrayReferenceOperator)
    {
        enum class Flags
        {
            Zero,
            One,
            Two,
            Three,
            Four
        };

        Log::Comment(L"Start with no bits set");
        til::enumset<Flags> flags;
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());

        Log::Comment(L"Test bit 3 reference is false");
        auto reference = flags[Flags::Three];
        VERIFY_IS_FALSE(reference);
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());

        Log::Comment(L"Set bit 3 reference to true");
        flags.set(Flags::Three);
        VERIFY_IS_TRUE(reference);
        VERIFY_ARE_EQUAL(0b01000u, flags.to_ulong());

        Log::Comment(L"Reset bit 3 reference to false");
        flags.reset(Flags::Three);
        VERIFY_IS_FALSE(reference);
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());

        Log::Comment(L"Flip bit 3 reference to true");
        reference.flip();
        VERIFY_IS_TRUE(reference);
        VERIFY_ARE_EQUAL(0b01000u, flags.to_ulong());

        Log::Comment(L"Flip bit 3 reference back to false");
        reference.flip();
        VERIFY_IS_FALSE(reference);
        VERIFY_ARE_EQUAL(0b00000u, flags.to_ulong());
    }
};
