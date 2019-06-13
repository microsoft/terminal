// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "../buffer/out/textBuffer.hpp"
#include "../buffer/out/textBufferCellIterator.hpp"
#include "../buffer/out/textBufferTextIterator.hpp"
#include "../buffer/out/CharRow.hpp"

#include "input.h"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class TextBufferIteratorTests
{
    CommonState* m_state;

    TEST_CLASS(TextBufferIteratorTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state->PrepareNewTextBufferInfo();

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();

        return true;
    }

    template<typename T>
    void BoolOperatorTestHelper()
    {
        const auto it = GetIterator<T>();
        VERIFY_IS_TRUE(it);

        const auto& outputBuffer = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer();
        const auto size = outputBuffer.GetBufferSize().Dimensions();
        T itInvalidPos(it);
        itInvalidPos._exceeded = true;
        VERIFY_IS_FALSE(itInvalidPos);
    }

    TEST_METHOD(BoolOperatorText);
    TEST_METHOD(BoolOperatorCell);

    template<typename T>
    void EqualsOperatorTestHelper()
    {
        const auto it = GetIterator<T>();
        const auto it2 = GetIterator<T>();

        VERIFY_ARE_EQUAL(it, it2);
    }

    TEST_METHOD(EqualsOperatorText);
    TEST_METHOD(EqualsOperatorCell);

    template<typename T>
    void NotEqualsOperatorTestHelper()
    {
        const auto it = GetIterator<T>();

        COORD oneOff = it._pos;
        oneOff.X++;
        const auto it2 = GetIteratorAt<T>(oneOff);

        VERIFY_ARE_NOT_EQUAL(it, it2);
    }

    TEST_METHOD(NotEqualsOperatorText);
    TEST_METHOD(NotEqualsOperatorCell);

    template<typename T>
    void PlusEqualsOperatorTestHelper()
    {
        auto it = GetIterator<T>();

        ptrdiff_t diffUnit = 3;
        COORD expectedPos = it._pos;
        expectedPos.X += gsl::narrow<SHORT>(diffUnit);
        const auto itExpected = GetIteratorAt<T>(expectedPos);

        it += diffUnit;

        VERIFY_ARE_EQUAL(itExpected, it);
    }

    TEST_METHOD(PlusEqualsOperatorText);
    TEST_METHOD(PlusEqualsOperatorCell);

    template<typename T>
    void MinusEqualsOperatorTestHelper()
    {
        auto itExpected = GetIteratorWithAdvance<T>();

        ptrdiff_t diffUnit = 3;
        COORD pos = itExpected._pos;
        pos.X += gsl::narrow<SHORT>(diffUnit);
        auto itOffset = GetIteratorAt<T>(pos);

        itOffset -= diffUnit;

        VERIFY_ARE_EQUAL(itExpected, itOffset);
    }

    TEST_METHOD(MinusEqualsOperatorText);
    TEST_METHOD(MinusEqualsOperatorCell);

    template<typename T>
    void PrefixPlusPlusOperatorTestHelper()
    {
        auto itActual = GetIterator<T>();

        COORD expectedPos = itActual._pos;
        expectedPos.X++;
        const auto itExpected = GetIteratorAt<T>(expectedPos);

        ++itActual;

        VERIFY_ARE_EQUAL(itExpected, itActual);
    }

    TEST_METHOD(PrefixPlusPlusOperatorText);
    TEST_METHOD(PrefixPlusPlusOperatorCell);

    template<typename T>
    void PrefixMinusMinusOperatorTestHelper()
    {
        const auto itExpected = GetIteratorWithAdvance<T>();

        COORD pos = itExpected._pos;
        pos.X++;
        auto itActual = GetIteratorAt<T>(pos);

        --itActual;

        VERIFY_ARE_EQUAL(itExpected, itActual);
    }

    TEST_METHOD(PrefixMinusMinusOperatorText);
    TEST_METHOD(PrefixMinusMinusOperatorCell);

    template<typename T>
    void PostfixPlusPlusOperatorTestHelper()
    {
        auto it = GetIterator<T>();

        COORD expectedPos = it._pos;
        expectedPos.X++;
        const auto itExpected = GetIteratorAt<T>(expectedPos);

        ++it;

        VERIFY_ARE_EQUAL(itExpected, it);
    }

    TEST_METHOD(PostfixPlusPlusOperatorText);
    TEST_METHOD(PostfixPlusPlusOperatorCell);

    template<typename T>
    void PostfixMinusMinusOperatorTestHelper()
    {
        const auto itExpected = GetIteratorWithAdvance<T>();

        COORD pos = itExpected._pos;
        pos.X++;
        auto itActual = GetIteratorAt<T>(pos);

        itActual--;

        VERIFY_ARE_EQUAL(itExpected, itActual);
    }

    TEST_METHOD(PostfixMinusMinusOperatorText);
    TEST_METHOD(PostfixMinusMinusOperatorCell);

    template<typename T>
    void PlusOperatorTestHelper()
    {
        auto it = GetIterator<T>();

        ptrdiff_t diffUnit = 3;
        COORD expectedPos = it._pos;
        expectedPos.X += gsl::narrow<SHORT>(diffUnit);
        const auto itExpected = GetIteratorAt<T>(expectedPos);

        const auto itActual = it + diffUnit;

        VERIFY_ARE_EQUAL(itExpected, itActual);
    }

    TEST_METHOD(PlusOperatorText);
    TEST_METHOD(PlusOperatorCell);

    template<typename T>
    void MinusOperatorTestHelper()
    {
        auto itExpected = GetIteratorWithAdvance<T>();

        ptrdiff_t diffUnit = 3;
        COORD pos = itExpected._pos;
        pos.X += gsl::narrow<SHORT>(diffUnit);
        auto itOffset = GetIteratorAt<T>(pos);

        const auto itActual = itOffset - diffUnit;

        VERIFY_ARE_EQUAL(itExpected, itActual);
    }

    TEST_METHOD(MinusOperatorText);
    TEST_METHOD(MinusOperatorCell);

    template<typename T>
    void DifferenceOperatorTestHelper()
    {
        const ptrdiff_t expected(3);
        auto it = GetIterator<T>();
        auto it2 = it + expected;

        const ptrdiff_t actual = it2 - it;
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(DifferenceOperatorText);
    TEST_METHOD(DifferenceOperatorCell);

    TEST_METHOD(AsCharInfoCell);

    TEST_METHOD(DereferenceOperatorText);
    TEST_METHOD(DereferenceOperatorCell);

    TEST_METHOD(ConstructedNoLimit);
    TEST_METHOD(ConstructedLimits);
};

template<typename T>
T GetIterator()
{
}

template<typename T>
T GetIteratorAt(COORD at)
{
}

template<typename T>
T GetIteratorWithAdvance()
{
}

template<>
TextBufferCellIterator GetIteratorAt<TextBufferCellIterator>(COORD at)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& outputBuffer = gci.GetActiveOutputBuffer();
    return outputBuffer.GetCellDataAt(at);
}

template<>
TextBufferCellIterator GetIterator<TextBufferCellIterator>()
{
    return GetIteratorAt<TextBufferCellIterator>({ 0 });
}

template<>
TextBufferCellIterator GetIteratorWithAdvance<TextBufferCellIterator>()
{
    return GetIteratorAt<TextBufferCellIterator>({ 5, 5 });
}

template<>
TextBufferTextIterator GetIteratorAt<TextBufferTextIterator>(COORD at)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& outputBuffer = gci.GetActiveOutputBuffer();
    return outputBuffer.GetTextDataAt(at);
}

template<>
TextBufferTextIterator GetIterator<TextBufferTextIterator>()
{
    return GetIteratorAt<TextBufferTextIterator>({ 0 });
}

template<>
TextBufferTextIterator GetIteratorWithAdvance<TextBufferTextIterator>()
{
    return GetIteratorAt<TextBufferTextIterator>({ 5, 5 });
}

void TextBufferIteratorTests::BoolOperatorText()
{
    BoolOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::BoolOperatorCell()
{
    BoolOperatorTestHelper<TextBufferCellIterator>();

    Log::Comment(L"For cells, also check incrementing past the end.");
    const auto& outputBuffer = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer();
    const auto size = outputBuffer.GetBufferSize().Dimensions();
    TextBufferCellIterator it(outputBuffer.GetTextBuffer(), { size.X - 1, size.Y - 1 });
    VERIFY_IS_TRUE(it);
    it++;
    VERIFY_IS_FALSE(it);
}

void TextBufferIteratorTests::EqualsOperatorText()
{
    EqualsOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::EqualsOperatorCell()
{
    EqualsOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::NotEqualsOperatorText()
{
    NotEqualsOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::NotEqualsOperatorCell()
{
    NotEqualsOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PlusEqualsOperatorText()
{
    PlusEqualsOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PlusEqualsOperatorCell()
{
    PlusEqualsOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::MinusEqualsOperatorText()
{
    MinusEqualsOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::MinusEqualsOperatorCell()
{
    MinusEqualsOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PrefixPlusPlusOperatorText()
{
    PrefixPlusPlusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PrefixPlusPlusOperatorCell()
{
    PrefixPlusPlusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PrefixMinusMinusOperatorText()
{
    PrefixMinusMinusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PrefixMinusMinusOperatorCell()
{
    PrefixMinusMinusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PostfixPlusPlusOperatorText()
{
    PostfixPlusPlusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PostfixPlusPlusOperatorCell()
{
    PostfixPlusPlusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PostfixMinusMinusOperatorText()
{
    PostfixMinusMinusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PostfixMinusMinusOperatorCell()
{
    PostfixMinusMinusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::PlusOperatorText()
{
    PlusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::PlusOperatorCell()
{
    PlusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::MinusOperatorText()
{
    MinusOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::MinusOperatorCell()
{
    MinusOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::DifferenceOperatorText()
{
    DifferenceOperatorTestHelper<TextBufferTextIterator>();
}

void TextBufferIteratorTests::DifferenceOperatorCell()
{
    DifferenceOperatorTestHelper<TextBufferCellIterator>();
}

void TextBufferIteratorTests::AsCharInfoCell()
{
    m_state->FillTextBuffer();
    const auto it = GetIterator<TextBufferCellIterator>();
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    const auto& outputBuffer = gci.GetActiveOutputBuffer();

    const auto& row = outputBuffer._textBuffer->GetRowByOffset(it._pos.Y);

    const auto wcharExpected = *row.GetCharRow().GlyphAt(it._pos.X).begin();
    const auto attrExpected = row.GetAttrRow().GetAttrByColumn(it._pos.X);

    const auto cellActual = gci.AsCharInfo(*it);
    const auto wcharActual = cellActual.Char.UnicodeChar;
    const auto attrActual = it->TextAttr();

    VERIFY_ARE_EQUAL(wcharExpected, wcharActual);
    VERIFY_ARE_EQUAL(attrExpected, attrActual);
}

void TextBufferIteratorTests::DereferenceOperatorText()
{
    m_state->FillTextBuffer();
    const auto it = GetIterator<TextBufferTextIterator>();

    const auto& outputBuffer = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer();

    const auto& row = outputBuffer._textBuffer->GetRowByOffset(it._pos.Y);

    const auto wcharExpected = row.GetCharRow().GlyphAt(it._pos.X);
    const auto wcharActual = *it;

    VERIFY_ARE_EQUAL(*wcharExpected.begin(), *wcharActual.begin());
}

void TextBufferIteratorTests::DereferenceOperatorCell()
{
    m_state->FillTextBuffer();
    const auto it = GetIterator<TextBufferCellIterator>();

    const auto& outputBuffer = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer();

    const auto& row = outputBuffer._textBuffer->GetRowByOffset(it._pos.Y);

    const auto textExpected = (std::wstring_view)row.GetCharRow().GlyphAt(it._pos.X);
    const auto dbcsExpected = row.GetCharRow().DbcsAttrAt(it._pos.X);
    const auto attrExpected = row.GetAttrRow().GetAttrByColumn(it._pos.X).GetLegacyAttributes();

    const auto cellActual = *it;
    const auto textActual = cellActual.Chars();
    const auto dbcsActual = cellActual.DbcsAttr();
    const auto attrActual = cellActual.TextAttr();

    VERIFY_ARE_EQUAL(String(textExpected.data(), (int)textExpected.size()), String(textActual.data(), (int)textActual.size()));
    VERIFY_ARE_EQUAL(dbcsExpected, dbcsActual);
    VERIFY_ARE_EQUAL(attrExpected, attrActual);
}

void TextBufferIteratorTests::ConstructedNoLimit()
{
    m_state->FillTextBuffer();

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& outputBuffer = gci.GetActiveOutputBuffer();
    const auto& textBuffer = outputBuffer.GetTextBuffer();
    const auto& bufferSize = textBuffer.GetSize();

    TextBufferCellIterator it(textBuffer, { 0 });

    VERIFY_IS_TRUE(it, L"Iterator is valid.");
    VERIFY_ARE_EQUAL(bufferSize, it._bounds, L"Bounds match the bounds of the text buffer.");

    const auto totalBufferDistance = bufferSize.Width() * bufferSize.Height();

    // Advance buffer to one before the end.
    it += (totalBufferDistance - 1);
    VERIFY_IS_TRUE(it, L"Iterator is still valid.");

    // Advance over the end.
    it++;
    VERIFY_IS_FALSE(it, L"Iterator invalid now.");

    // Verify throws for out of range.
    VERIFY_THROWS_SPECIFIC(TextBufferCellIterator(textBuffer, { -1, -1 }), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
}

void TextBufferIteratorTests::ConstructedLimits()
{
    m_state->FillTextBuffer();

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& outputBuffer = gci.GetActiveOutputBuffer();
    const auto& textBuffer = outputBuffer.GetTextBuffer();

    SMALL_RECT limits;
    limits.Top = 1;
    limits.Bottom = 1;
    limits.Left = 3;
    limits.Right = 5;
    const auto viewport = Microsoft::Console::Types::Viewport::FromInclusive(limits);

    COORD pos;
    pos.X = limits.Left;
    pos.Y = limits.Top;

    TextBufferCellIterator it(textBuffer, pos, viewport);

    VERIFY_IS_TRUE(it, L"Iterator is valid.");
    VERIFY_ARE_EQUAL(viewport, it._bounds, L"Bounds match the bounds given.");

    const auto totalBufferDistance = viewport.Width() * viewport.Height();

    // Advance buffer to one before the end.
    it += (totalBufferDistance - 1);
    VERIFY_IS_TRUE(it, L"Iterator is still valid.");

    // Advance over the end.
    it++;
    VERIFY_IS_FALSE(it, L"Iterator invalid now.");

    // Verify throws for out of range.
    VERIFY_THROWS_SPECIFIC(TextBufferCellIterator(textBuffer,
                                                  { 0 },
                                                  viewport),
                           wil::ResultException,
                           [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });

    // Verify throws for limit not inside buffer
    const auto bufferSize = textBuffer.GetSize();
    VERIFY_THROWS_SPECIFIC(TextBufferCellIterator(textBuffer,
                                                  pos,
                                                  Microsoft::Console::Types::Viewport::FromInclusive(bufferSize.ToExclusive())),
                           wil::ResultException,
                           [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
}
