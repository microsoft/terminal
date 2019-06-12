// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "../buffer/out/textBuffer.hpp"

#include "input.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace WEX
{
    namespace TestExecution
    {
        template<>
        class VerifyOutputTraits<TextAttributeRun>
        {
        public:
            static WEX::Common::NoThrowString ToString(const TextAttributeRun& tar)
            {
                return WEX::Common::NoThrowString().Format(
                    L"Length:%d, attr:%s",
                    tar.GetLength(),
                    VerifyOutputTraits<TextAttribute>::ToString(tar.GetAttributes()).GetBuffer());
            }
        };

        template<>
        class VerifyCompareTraits<TextAttributeRun, TextAttributeRun>
        {
        public:
            static bool AreEqual(const TextAttributeRun& expected, const TextAttributeRun& actual)
            {
                return expected.GetAttributes() == actual.GetAttributes() &&
                       expected.GetLength() == actual.GetLength();
            }

            static bool AreSame(const TextAttributeRun& expected, const TextAttributeRun& actual)
            {
                return &expected == &actual;
            }

            static bool IsLessThan(const TextAttributeRun&, const TextAttributeRun&) = delete;

            static bool IsGreaterThan(const TextAttributeRun&, const TextAttributeRun&) = delete;

            static bool IsNull(const TextAttributeRun& object)
            {
                return object.GetAttributes().IsLegacy() && object.GetAttributes().GetLegacyAttributes() == 0 &&
                       object.GetLength() == 0;
            }
        };
    }
}

class AttrRowTests
{
    ATTR_ROW* pSingle;
    ATTR_ROW* pChain;

    short _sDefaultLength = 80;
    short _sDefaultChainLength = 6;

    short sChainSegLength;
    short sChainLeftover;
    short sChainSegmentsNeeded;

    WORD __wDefaultAttr = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
    WORD __wDefaultChainAttr = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY;
    TextAttribute _DefaultAttr = TextAttribute(__wDefaultAttr);
    TextAttribute _DefaultChainAttr = TextAttribute(__wDefaultChainAttr);

    TEST_CLASS(AttrRowTests);

    TEST_METHOD_SETUP(MethodSetup)
    {
        pSingle = new ATTR_ROW(_sDefaultLength, _DefaultAttr);

        // Segment length is the expected length divided by the row length
        // E.g. row of 80, 4 segments, 20 segment length each
        sChainSegLength = _sDefaultLength / _sDefaultChainLength;

        // Leftover is spaces that don't fit evenly
        // E.g. row of 81, 4 segments, 1 leftover length segment
        sChainLeftover = _sDefaultLength % _sDefaultChainLength;

        // Start with the number of segments we expect
        sChainSegmentsNeeded = _sDefaultChainLength;

        // If we had a remainder, add one more segment
        if (sChainLeftover)
        {
            sChainSegmentsNeeded++;
        }

        // Create the chain
        pChain = new ATTR_ROW(_sDefaultLength, _DefaultAttr);
        pChain->_list.resize(sChainSegmentsNeeded);

        // Attach all chain segments that are even multiples of the row length
        for (short iChain = 0; iChain < _sDefaultChainLength; iChain++)
        {
            TextAttributeRun* pRun = &pChain->_list[iChain];

            pRun->SetAttributesFromLegacy(iChain); // Just use the chain position as the value
            pRun->SetLength(sChainSegLength);
        }

        if (sChainLeftover > 0)
        {
            // If we had a leftover, then this chain is one longer than we expected (the default length)
            // So use it as the index (because indicies start at 0)
            TextAttributeRun* pRun = &pChain->_list[_sDefaultChainLength];

            pRun->SetAttributes(_DefaultChainAttr);
            pRun->SetLength(sChainLeftover);
        }

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        delete pSingle;

        delete pChain;

        return true;
    }

    TEST_METHOD(TestInitialize)
    {
        // Properties needed for test
        const WORD wAttr = FOREGROUND_RED | BACKGROUND_BLUE;
        TextAttribute attr = TextAttribute(wAttr);
        // Cases to test
        ATTR_ROW* pTestItems[]{ pSingle, pChain };

        // Loop cases
        for (UINT iIndex = 0; iIndex < ARRAYSIZE(pTestItems); iIndex++)
        {
            ATTR_ROW* pUnderTest = pTestItems[iIndex];

            pUnderTest->Reset(attr);

            VERIFY_ARE_EQUAL(pUnderTest->_list.size(), 1u);
            VERIFY_ARE_EQUAL(pUnderTest->_list[0].GetAttributes(), attr);
            VERIFY_ARE_EQUAL(pUnderTest->_list[0].GetLength(), (unsigned int)_sDefaultLength);
        }
    }

    // Routine Description:
    // - Packs an array of words representing attributes into the more compact storage form used by the row.
    // Arguments:
    // - rgAttrs - Array of words representing the attribute associated with each character position in the row.
    // - cRowLength - Length of preceeding array.
    // - outAttrRun - reference to unique_ptr that will contain packed attr run on success.
    // Return Value:
    // - Success if success. Buffer too small if row length is incorrect.
    HRESULT PackAttrs(_In_reads_(cRowLength) const TextAttribute* const rgAttrs,
                      const size_t cRowLength,
                      _Inout_ std::unique_ptr<TextAttributeRun[]>& outAttrRun,
                      _Out_ size_t* const cOutAttrRun)
    {
        NTSTATUS status = STATUS_SUCCESS;

        if (cRowLength == 0)
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }

        if (NT_SUCCESS(status))
        {
            // first count up the deltas in the array
            size_t cDeltas = 1;

            const TextAttribute* pPrevAttr = &rgAttrs[0];

            for (size_t i = 1; i < cRowLength; i++)
            {
                const TextAttribute* pCurAttr = &rgAttrs[i];

                if (*pCurAttr != *pPrevAttr)
                {
                    cDeltas++;
                }

                pPrevAttr = pCurAttr;
            }

            // This whole situation was too complicated with a one off holder for one row run
            // new method:
            // delete the old buffer
            // make a new buffer, one run + one run for each change
            // set the values for each run one run index at a time

            std::unique_ptr<TextAttributeRun[]> attrRun = std::make_unique<TextAttributeRun[]>(cDeltas);
            status = NT_TESTNULL(attrRun.get());
            if (NT_SUCCESS(status))
            {
                TextAttributeRun* pCurrentRun = attrRun.get();
                pCurrentRun->SetAttributes(rgAttrs[0]);
                pCurrentRun->SetLength(1);
                for (size_t i = 1; i < cRowLength; i++)
                {
                    if (pCurrentRun->GetAttributes() == rgAttrs[i])
                    {
                        pCurrentRun->SetLength(pCurrentRun->GetLength() + 1);
                    }
                    else
                    {
                        pCurrentRun++;
                        pCurrentRun->SetAttributes(rgAttrs[i]);
                        pCurrentRun->SetLength(1);
                    }
                }
                attrRun.swap(outAttrRun);
                *cOutAttrRun = cDeltas;
            }
        }

        return HRESULT_FROM_NT(status);
    }

    NoThrowString LogRunElement(_In_ TextAttributeRun& run)
    {
        return NoThrowString().Format(L"%wc%d", run.GetAttributes().GetLegacyAttributes(), run.GetLength());
    }

    void LogChain(_In_ PCWSTR pwszPrefix,
                  std::vector<TextAttributeRun>& chain)
    {
        NoThrowString str(pwszPrefix);

        if (chain.size() > 0)
        {
            str.Append(LogRunElement(chain[0]));

            for (size_t i = 1; i < chain.size(); i++)
            {
                str.AppendFormat(L"->%s", (const wchar_t*)(LogRunElement(chain[i])));
            }
        }

        Log::Comment(str);
    }

    void DoTestInsertAttrRuns(UINT& uiStartPos, WORD& ch1, UINT& uiChar1Length, WORD& ch2, UINT& uiChar2Length)
    {
        Log::Comment(String().Format(L"StartPos: %d, Char1: %wc, Char1Length: %d, Char2: %wc, Char2Length: %d",
                                     uiStartPos,
                                     ch1,
                                     uiChar1Length,
                                     ch2,
                                     uiChar2Length));

        bool const fUseStr2 = (ch2 != L'0');

        // Set up our "original row" that we are going to try to insert into.
        // This will represent a 10 column run of R3->B5->G2 that we will use for all tests.
        ATTR_ROW originalRow{ static_cast<UINT>(_sDefaultLength), _DefaultAttr };
        originalRow._list.resize(3);
        originalRow._cchRowWidth = 10;
        originalRow._list[0].SetAttributesFromLegacy('R');
        originalRow._list[0].SetLength(3);
        originalRow._list[1].SetAttributesFromLegacy('B');
        originalRow._list[1].SetLength(5);
        originalRow._list[2].SetAttributesFromLegacy('G');
        originalRow._list[2].SetLength(2);
        LogChain(L"Original: ", originalRow._list);

        // Set up our "insertion run"
        size_t cInsertRow = 1;
        if (fUseStr2)
        {
            cInsertRow++;
        }

        std::vector<TextAttributeRun> insertRow;
        insertRow.resize(cInsertRow);
        insertRow[0].SetAttributesFromLegacy(ch1);
        insertRow[0].SetLength(uiChar1Length);
        if (fUseStr2)
        {
            insertRow[1].SetAttributesFromLegacy(ch2);
            insertRow[1].SetLength(uiChar2Length);
        }

        LogChain(L"Insert: ", insertRow);
        Log::Comment(NoThrowString().Format(L"At Index: %d", uiStartPos));

        UINT uiTotalLength = uiChar1Length;
        if (fUseStr2)
        {
            uiTotalLength += uiChar2Length;
        }

        VERIFY_IS_TRUE((uiStartPos + uiTotalLength) >= 1); // assert we won't underflow.
        UINT const uiEndPos = uiStartPos + uiTotalLength - 1;

        // Calculate our expected final/result run by unpacking original, laying our insertion on it at the index
        // then using our pack function to repack it.
        // This method is easy to understand and very reliable, but its performance is bad.
        // The InsertAttrRuns method we test against below is hard to understand but very high performance in production.

        // - 1. Unpack
        std::vector<TextAttribute> unpackedOriginal = { originalRow.begin(), originalRow.end() };

        // - 2. Overlay insertion
        UINT uiInsertedCount = 0;
        UINT uiInsertIndex = 0;

        // --- Walk through the unpacked run from start to end....
        for (UINT uiUnpackedIndex = uiStartPos; uiUnpackedIndex <= uiEndPos; uiUnpackedIndex++)
        {
            // Pull the item from the insert run to analyze.
            TextAttributeRun run = insertRow[uiInsertIndex];

            // Copy the attribute from the run into the unpacked array
            unpackedOriginal[uiUnpackedIndex] = run.GetAttributes();

            // Increment how many times we've copied this particular portion of the run
            uiInsertedCount++;

            // If we've now inserted enough of them to match the length, advance the insert index and reset the counter.
            if (uiInsertedCount >= run.GetLength())
            {
                uiInsertIndex++;
                uiInsertedCount = 0;
            }
        }

        // - 3. Pack.
        std::unique_ptr<TextAttributeRun[]> packedRun;
        size_t cPackedRun = 0;
        VERIFY_SUCCEEDED(PackAttrs(unpackedOriginal.data(), originalRow._cchRowWidth, packedRun, &cPackedRun));

        // Now send parameters into InsertAttrRuns and get its opinion on the subject.
        VERIFY_SUCCEEDED(originalRow.InsertAttrRuns({ insertRow.data(), insertRow.size() }, uiStartPos, uiEndPos, (UINT)originalRow._cchRowWidth));

        // Compare and ensure that the expected and actual match.
        VERIFY_ARE_EQUAL(cPackedRun, originalRow._list.size(), L"Ensure that number of array elements required for RLE are the same.");

        std::vector<TextAttributeRun> packedRunExpected;
        std::copy_n(packedRun.get(), cPackedRun, std::back_inserter(packedRunExpected));

        LogChain(L"Expected: ", packedRunExpected);
        LogChain(L"Actual: ", originalRow._list);

        for (size_t testIndex = 0; testIndex < cPackedRun; testIndex++)
        {
            VERIFY_ARE_EQUAL(packedRun[testIndex], originalRow._list[testIndex]);
        }
    }

    TEST_METHOD(TestInsertAttrRunsSingle)
    {
        UINT const uiTestRunLength = 10;

        UINT uiStartPos = 0;
        WORD ch1 = L'0';
        UINT uiChar1Length = 0;
        WORD ch2 = L'0';
        UINT uiChar2Length = 0;

        Log::Comment(L"Test inserting a single item of a variable length into the run.");
        WORD rgch1Options[] = { L'X', L'R', L'G', L'B' };
        for (size_t iCh1Option = 0; iCh1Option < ARRAYSIZE(rgch1Options); iCh1Option++)
        {
            ch1 = rgch1Options[iCh1Option];
            for (UINT iCh1Length = 1; iCh1Length <= uiTestRunLength; iCh1Length++)
            {
                uiChar1Length = iCh1Length;

                // We can't try to insert a run that's longer than would fit.
                // If the run is of length 10 and we're trying to insert a length of 10,
                // we can only insert at position 0.
                // For the run length of 10 and an insert length of 9, we can try positions 0 and 1.
                // And so on...
                UINT const uiMaxPos = uiTestRunLength - uiChar1Length;

                for (UINT iStartPos = 0; iStartPos < uiMaxPos; iStartPos++)
                {
                    uiStartPos = iStartPos;

                    DoTestInsertAttrRuns(uiStartPos, ch1, uiChar1Length, ch2, uiChar2Length);
                }
            }
        }
    }

    TEST_METHOD(TestInsertAttrRunsMultiple)
    {
        UINT const uiTestRunLength = 10;

        UINT uiStartPos = 0;
        WORD ch1 = L'0';
        UINT uiChar1Length = 0;
        WORD ch2 = L'0';
        UINT uiChar2Length = 0;

        Log::Comment(L"Test inserting a multiple item run with each piece having variable length into the existing run.");
        WORD rgch1Options[] = { L'X', L'R', L'G', L'B' };
        for (size_t iCh1Option = 0; iCh1Option < ARRAYSIZE(rgch1Options); iCh1Option++)
        {
            ch1 = rgch1Options[iCh1Option];

            UINT const uiMaxCh1Length = uiTestRunLength - 1; // leave at least 1 space for the second piece of the inser trun.
            for (UINT iCh1Length = 1; iCh1Length <= uiMaxCh1Length; iCh1Length++)
            {
                uiChar1Length = iCh1Length;

                WORD rgch2Options[] = { L'Y' };
                for (size_t iCh2Option = 0; iCh2Option < ARRAYSIZE(rgch2Options); iCh2Option++)
                {
                    ch2 = rgch2Options[iCh2Option];

                    // When choosing the length of the second item, it can't be bigger than the remaining space in the run
                    // when accounting for the length of the first piece chosen.
                    // For example if the total run length is 10 and the first piece chosen was 8 long,
                    // the second piece can only be 1 or 2 long.
                    UINT const uiMaxCh2Length = uiTestRunLength - uiMaxCh1Length;
                    for (UINT iCh2Length = 1; iCh2Length <= uiMaxCh2Length; iCh2Length++)
                    {
                        uiChar2Length = iCh2Length;

                        // We can't try to insert a run that's longer than would fit.
                        // If the run is of length 10 and we're trying to insert a total length of 10,
                        // we can only insert at position 0.
                        // For the run length of 10 and an insert length of 9, we can try positions 0 and 1.
                        // And so on...
                        UINT const uiMaxPos = uiTestRunLength - (uiChar1Length + uiChar2Length);

                        for (UINT iStartPos = 0; iStartPos <= uiMaxPos; iStartPos++)
                        {
                            uiStartPos = iStartPos;

                            DoTestInsertAttrRuns(uiStartPos, ch1, uiChar1Length, ch2, uiChar2Length);
                        }
                    }
                }
            }
        }
    }

    TEST_METHOD(TestUnpackAttrs)
    {
        Log::Comment(L"Checking unpack of a single color for the entire length");
        {
            const std::vector<TextAttribute> attrs{ pSingle->begin(), pSingle->end() };

            for (auto& attr : attrs)
            {
                VERIFY_ARE_EQUAL(attr, _DefaultAttr);
            }
        }

        Log::Comment(L"Checking unpack of the multiple color chain");

        const std::vector<TextAttribute> attrs{ pChain->begin(), pChain->end() };

        short cChainRun = 0; // how long we've been looking at the current piece of the chain
        short iChainSegIndex = 0; // which piece of the chain we should be on right now

        for (auto& attr : attrs)
        {
            // by default the chain was assembled above to have the chain segment index be the attribute
            TextAttribute MatchingAttr = TextAttribute(iChainSegIndex);

            // However, if the index is greater than the expected chain length, a remainder piece was made with a default attribute
            if (iChainSegIndex >= _sDefaultChainLength)
            {
                MatchingAttr = _DefaultChainAttr;
            }

            VERIFY_ARE_EQUAL(attr, MatchingAttr);

            // Add to the chain run
            cChainRun++;

            // If the chain run is greater than the length the segments were specified to be
            if (cChainRun >= sChainSegLength)
            {
                // reset to 0
                cChainRun = 0;

                // move to the next chain segment down the line
                iChainSegIndex++;
            }
        }
    }

    TEST_METHOD(TestSetAttrToEnd)
    {
        const WORD wTestAttr = FOREGROUND_BLUE | BACKGROUND_GREEN;
        TextAttribute TestAttr = TextAttribute(wTestAttr);

        Log::Comment(L"FIRST: Set index to > 0 to test making/modifying chains");
        const short iTestIndex = 50;
        VERIFY_IS_TRUE(iTestIndex >= 0 && iTestIndex < _sDefaultLength);

        Log::Comment(L"SetAttrToEnd for single color applied to whole string.");
        pSingle->SetAttrToEnd(iTestIndex, TestAttr);

        // Was 1 (single), should now have 2 segments
        VERIFY_ARE_EQUAL(pSingle->_list.size(), 2u);

        VERIFY_ARE_EQUAL(pSingle->_list[0].GetAttributes(), _DefaultAttr);
        VERIFY_ARE_EQUAL(pSingle->_list[0].GetLength(), (unsigned int)(_sDefaultLength - (_sDefaultLength - iTestIndex)));

        VERIFY_ARE_EQUAL(pSingle->_list[1].GetAttributes(), TestAttr);
        VERIFY_ARE_EQUAL(pSingle->_list[1].GetLength(), (unsigned int)(_sDefaultLength - iTestIndex));

        Log::Comment(L"SetAttrToEnd for existing chain of multiple colors.");
        pChain->SetAttrToEnd(iTestIndex, TestAttr);

        // From 7 segments down to 5.
        VERIFY_ARE_EQUAL(pChain->_list.size(), 5u);

        // Verify chain colors and lengths
        VERIFY_ARE_EQUAL(TextAttribute(0), pChain->_list[0].GetAttributes());
        VERIFY_ARE_EQUAL(pChain->_list[0].GetLength(), (unsigned int)13);

        VERIFY_ARE_EQUAL(TextAttribute(1), pChain->_list[1].GetAttributes());
        VERIFY_ARE_EQUAL(pChain->_list[1].GetLength(), (unsigned int)13);

        VERIFY_ARE_EQUAL(TextAttribute(2), pChain->_list[2].GetAttributes());
        VERIFY_ARE_EQUAL(pChain->_list[2].GetLength(), (unsigned int)13);

        VERIFY_ARE_EQUAL(TextAttribute(3), pChain->_list[3].GetAttributes());
        VERIFY_ARE_EQUAL(pChain->_list[3].GetLength(), (unsigned int)11);

        VERIFY_ARE_EQUAL(TestAttr, pChain->_list[4].GetAttributes());
        VERIFY_ARE_EQUAL(pChain->_list[4].GetLength(), (unsigned int)30);

        Log::Comment(L"SECOND: Set index to 0 to test replacing anything with a single");

        ATTR_ROW* pTestItems[]{ pSingle, pChain };

        for (UINT iIndex = 0; iIndex < ARRAYSIZE(pTestItems); iIndex++)
        {
            ATTR_ROW* pUnderTest = pTestItems[iIndex];

            pUnderTest->SetAttrToEnd(0, TestAttr);

            // should be down to 1 attribute set from beginning to end of string
            VERIFY_ARE_EQUAL(pUnderTest->_list.size(), 1u);

            // singular pair should contain the color
            VERIFY_ARE_EQUAL(pUnderTest->_list[0].GetAttributes(), TestAttr);

            // and its length should be the length of the whole string
            VERIFY_ARE_EQUAL(pUnderTest->_list[0].GetLength(), (unsigned int)_sDefaultLength);
        }
    }

    TEST_METHOD(TestTotalLength)
    {
        ATTR_ROW* pTestItems[]{ pSingle, pChain };

        for (UINT iIndex = 0; iIndex < ARRAYSIZE(pTestItems); iIndex++)
        {
            ATTR_ROW* pUnderTest = pTestItems[iIndex];

            const size_t Result = pUnderTest->_cchRowWidth;

            VERIFY_ARE_EQUAL((short)Result, _sDefaultLength);
        }
    }

    TEST_METHOD(TestResize)
    {
        CommonState state;
        state.PrepareGlobalFont();
        state.PrepareGlobalScreenBuffer();

        pSingle->Resize(240);
        pChain->Resize(240);

        pSingle->Resize(255);
        pChain->Resize(255);

        pSingle->Resize(255);
        pChain->Resize(255);

        pSingle->Resize(60);
        pChain->Resize(60);

        pSingle->Resize(60);
        pChain->Resize(60);

        VERIFY_THROWS_SPECIFIC(pSingle->Resize(0), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        VERIFY_THROWS_SPECIFIC(pChain->Resize(0), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });

        state.CleanupGlobalScreenBuffer();
        state.CleanupGlobalFont();
    }
};
