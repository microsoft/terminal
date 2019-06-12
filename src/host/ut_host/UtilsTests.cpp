// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"

#include <time.h>

#include "utils.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class UtilsTests
{
    TEST_CLASS(UtilsTests);

    CommonState* m_state;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();

        UINT const seed = (UINT)time(NULL);
        Log::Comment(String().Format(L"Setting random seed to : %d", seed));
        srand(seed);

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        delete m_state;

        return true;
    }

    SHORT RandomShort()
    {
        SHORT s;

        do
        {
            s = (SHORT)rand() % SHORT_MAX;
        } while (s == 0i16);

        return s;
    }

    void FillBothCoordsSameRandom(COORD* pcoordA, COORD* pcoordB)
    {
        pcoordA->X = pcoordB->X = RandomShort();
        pcoordA->Y = pcoordB->Y = RandomShort();
    }

    void LogCoordinates(const COORD coordA, const COORD coordB)
    {
        Log::Comment(String().Format(L"Coordinates - A: (%d, %d) B: (%d, %d)", coordA.X, coordA.Y, coordB.X, coordB.Y));
    }

    void SubtractRandom(short& psValue)
    {
        SHORT const sRand = RandomShort();
        psValue -= gsl::narrow<SHORT>(std::max(sRand % psValue, 1));
    }

    TEST_METHOD(TestCompareCoords)
    {
        int result = 5; // not 1, 0, or -1
        COORD coordA;
        COORD coordB;

        // Set the buffer size to be able to accomodate large values.
        COORD coordMaxBuffer;
        coordMaxBuffer.X = SHORT_MAX;
        coordMaxBuffer.Y = SHORT_MAX;

        Log::Comment(L"#1: 0 case. Coords equal");
        FillBothCoordsSameRandom(&coordA, &coordB);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_ARE_EQUAL(result, 0);

        Log::Comment(L"#2: -1 case. A comes before B");
        Log::Comment(L"A. A left of B, same line");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordA.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_LESS_THAN(result, 0);

        Log::Comment(L"B. A above B, same column");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordA.Y);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_LESS_THAN(result, 0);

        Log::Comment(L"C. A up and to the left of B.");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordA.Y);
        SubtractRandom(coordA.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_LESS_THAN(result, 0);

        Log::Comment(L"D. A up and to the right of B.");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordA.Y);
        SubtractRandom(coordB.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_LESS_THAN(result, 0);

        Log::Comment(L"#3: 1 case. A comes after B");
        Log::Comment(L"A. A right of B, same line");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordB.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_GREATER_THAN(result, 0);

        Log::Comment(L"B. A below B, same column");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordB.Y);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_GREATER_THAN(result, 0);

        Log::Comment(L"C. A down and to the left of B");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordB.Y);
        SubtractRandom(coordA.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_GREATER_THAN(result, 0);

        Log::Comment(L"D. A down and to the right of B");
        FillBothCoordsSameRandom(&coordA, &coordB);
        SubtractRandom(coordB.Y);
        SubtractRandom(coordB.X);
        LogCoordinates(coordA, coordB);
        result = Utils::s_CompareCoords(coordMaxBuffer, coordA, coordB);
        VERIFY_IS_GREATER_THAN(result, 0);
    }
};
