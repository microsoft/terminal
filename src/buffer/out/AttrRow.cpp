// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "AttrRow.hpp"

// Routine Description:
// - constructor
// Arguments:
// - cchRowWidth - the length of the default text attribute
// - attr - the default text attribute
// Return Value:
// - constructed object
// Note: will throw exception if unable to allocate memory for text attribute storage
ATTR_ROW::ATTR_ROW(const UINT cchRowWidth, const TextAttribute attr)
{
    _list.push_back(TextAttributeRun(cchRowWidth, attr));
    _cchRowWidth = cchRowWidth;
}

// Routine Description:
// - Sets all properties of the ATTR_ROW to default values
// Arguments:
// - attr - The default text attributes to use on text in this row.
void ATTR_ROW::Reset(const TextAttribute attr)
{
    _list.clear();
    _list.push_back(TextAttributeRun(_cchRowWidth, attr));
}

// Routine Description:
// - Takes an existing row of attributes, and changes the length so that it fills the NewWidth.
//     If the new size is bigger, then the last attr is extended to fill the NewWidth.
//     If the new size is smaller, the runs are cut off to fit.
// Arguments:
// - oldWidth - The original width of the row.
// - newWidth - The new width of the row.
// Return Value:
// - <none>, throws exceptions on failures.
void ATTR_ROW::Resize(const size_t newWidth)
{
    THROW_HR_IF(E_INVALIDARG, 0 == newWidth);

    // Easy case. If the new row is longer, increase the length of the last run by how much new space there is.
    if (newWidth > _cchRowWidth)
    {
        // Get the attribute that covers the final column of old width.
        const auto runPos = FindAttrIndex(_cchRowWidth - 1, nullptr);
        auto& run = _list.at(runPos);

        // Extend its length by the additional columns we're adding.
        run.SetLength(run.GetLength() + newWidth - _cchRowWidth);

        // Store that the new total width we represent is the new width.
        _cchRowWidth = newWidth;
    }
    // harder case: new row is shorter.
    else
    {
        // Get the attribute that covers the final column of the new width
        size_t CountOfAttr = 0;
        const auto runPos = FindAttrIndex(newWidth - 1, &CountOfAttr);
        auto& run = _list.at(runPos);

        // CountOfAttr was given to us as "how many columns left from this point forward are covered by the returned run"
        // So if the original run was B5 covering a 5 size OldWidth and we have a NewWidth of 3
        // then when we called FindAttrIndex, it returned the B5 as the pIndexedRun and a 2 for how many more segments it covers
        // after and including the 3rd column.
        // B5-2 = B3, which is what we desire to cover the new 3 size buffer.
        run.SetLength(run.GetLength() - CountOfAttr + 1);

        // Store that the new total width we represent is the new width.
        _cchRowWidth = newWidth;

        // Erase segments after the one we just updated.
        _list.erase(_list.cbegin() + runPos + 1, _list.cend());

        // NOTE: Under some circumstances here, we have leftover run segments in memory or blank run segments
        // in memory. We're not going to waste time redimensioning the array in the heap. We're just noting that the useful
        // portions of it have changed.
    }
}

// Routine Description:
// - returns a copy of the TextAttribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// Return Value:
// - the text attribute at column
// Note:
// - will throw on error
TextAttribute ATTR_ROW::GetAttrByColumn(const size_t column) const
{
    return GetAttrByColumn(column, nullptr);
}

// Routine Description:
// - returns a copy of the TextAttribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// - pApplies - if given, fills how long this attribute will apply for
// Return Value:
// - the text attribute at column
// Note:
// - will throw on error
TextAttribute ATTR_ROW::GetAttrByColumn(const size_t column,
                                        size_t* const pApplies) const
{
    THROW_HR_IF(E_INVALIDARG, column >= _cchRowWidth);
    const auto runPos = FindAttrIndex(column, pApplies);
    return _list.at(runPos).GetAttributes();
}

// Routine Description:
// - reports how many runs we have stored (to be used for some optimizations
// Return Value:
// - Count of runs. 1 means we have 1 color to represent the entire row.
size_t ATTR_ROW::GetNumberOfRuns() const noexcept
{
    return _list.size();
}

// Routine Description:
// - This routine finds the nth attribute in this ATTR_ROW.
// Arguments:
// - index - which attribute to find
// - applies - on output, contains corrected length of indexed attr.
//             for example, if the attribute string was { 5, BLUE } and the requested
//             index was 3, CountOfAttr would be 2.
// Return Value:
// - const reference to attribute run object
size_t ATTR_ROW::FindAttrIndex(const size_t index, size_t* const pApplies) const
{
    FAIL_FAST_IF(!(index < _cchRowWidth)); // The requested index cannot be longer than the total length described by this set of Attrs.

    size_t cTotalLength = 0;

    FAIL_FAST_IF(!(_list.size() > 0)); // There should be a non-zero and positive number of items in the array.

    // Scan through the internal array from position 0 adding up the lengths that each attribute applies to
    auto runPos = _list.cbegin();
    do
    {
        cTotalLength += runPos->GetLength();

        if (cTotalLength > index)
        {
            // If we've just passed up the requested index with the length we added, break early
            break;
        }

        runPos++;
    } while (runPos < _list.cend());

    // we should have broken before falling out the while case.
    // if we didn't break, then this ATTR_ROW wasn't filled with enough attributes for the entire row of characters
    FAIL_FAST_IF(runPos >= _list.cend());

    // The remaining iterator position is the position of the attribute that is applicable at the position requested (index)
    // Calculate its remaining applicability if requested

    // The length on which the found attribute applies is the total length seen so far minus the index we were searching for.
    FAIL_FAST_IF(!(cTotalLength > index)); // The length of all attributes we counted up so far should be longer than the index requested or we'll underflow.

    if (nullptr != pApplies)
    {
        const auto attrApplies = cTotalLength - index;
        FAIL_FAST_IF(!(attrApplies > 0)); // An attribute applies for >0 characters
        // MSFT: 17130145 - will restore this and add a better assert to catch the real issue.
        //FAIL_FAST_IF(!(attrApplies <= _cchRowWidth)); // An attribute applies for a maximum of the total length available to us

        *pApplies = attrApplies;
    }

    return runPos - _list.cbegin();
}

// Routine Description:
// - Sets the attributes (colors) of all character positions from the given position through the end of the row.
// Arguments:
// - iStart - Starting index position within the row
// - attr - Attribute (color) to fill remaining characters with
// Return Value:
// - <none>
bool ATTR_ROW::SetAttrToEnd(const UINT iStart, const TextAttribute attr)
{
    size_t const length = _cchRowWidth - iStart;

    const TextAttributeRun run(length, attr);
    return SUCCEEDED(InsertAttrRuns({ &run, 1 }, iStart, _cchRowWidth - 1, _cchRowWidth));
}

// Routine Description:
// - Replaces all runs in the row with the given wToBeReplacedAttr with the new
//      attribute wReplaceWith. This method is used for replacing specifically
//      legacy attributes.
// Arguments:
// - wToBeReplacedAttr - the legacy attribute to replace in this row.
// - wReplaceWith - the new value for the matching runs' attributes.
// Return Value:
// <none>
void ATTR_ROW::ReplaceLegacyAttrs(_In_ WORD wToBeReplacedAttr, _In_ WORD wReplaceWith) noexcept
{
    TextAttribute ToBeReplaced;
    ToBeReplaced.SetFromLegacy(wToBeReplacedAttr);

    TextAttribute ReplaceWith;
    ReplaceWith.SetFromLegacy(wReplaceWith);

    ReplaceAttrs(ToBeReplaced, ReplaceWith);
}

// Method Description:
// - Replaces all runs in the row with the given toBeReplacedAttr with the new
//      attribute replaceWith.
// Arguments:
// - toBeReplacedAttr - the attribute to replace in this row.
// - replaceWith - the new value for the matching runs' attributes.
// Return Value:
// - <none>
void ATTR_ROW::ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith) noexcept
{
    for (auto& run : _list)
    {
        if (run.GetAttributes() == toBeReplacedAttr)
        {
            run.SetAttributes(replaceWith);
        }
    }
}

// Routine Description:
// - Takes a array of attribute runs, and inserts them into this row from startIndex to endIndex.
// - For example, if the current row was was [{4, BLUE}], the merge string
//   was [{ 2, RED }], with (StartIndex, EndIndex) = (1, 2),
//   then the row would modified to be = [{ 1, BLUE}, {2, RED}, {1, BLUE}].
// Arguments:
// - rgInsertAttrs - The array of attrRuns to merge into this row.
// - cInsertAttrs - The number of elements in rgInsertAttrs
// - iStart - The index in the row to place the array of runs.
// - iEnd - the final index of the merge runs
// - BufferWidth - the width of the row.
// Return Value:
// - STATUS_NO_MEMORY if there wasn't enough memory to insert the runs
//   otherwise STATUS_SUCCESS if we were successful.
[[nodiscard]] HRESULT ATTR_ROW::InsertAttrRuns(const std::basic_string_view<TextAttributeRun> newAttrs,
                                               const size_t iStart,
                                               const size_t iEnd,
                                               const size_t cBufferWidth)
{
    // Definitions:
    // Existing Run = The run length encoded color array we're already storing in memory before this was called.
    // Insert Run = The run length encoded color array that someone is asking us to inject into our stored memory run.
    // New Run = The run length encoded color array that we have to allocate and rebuild to store internally
    //           which will replace Existing Run at the end of this function.
    // Example:
    // cBufferWidth = 10.
    // Existing Run: R3 -> G5 -> B2
    // Insert Run: Y1 -> N1 at iStart = 5 and iEnd = 6
    //            (rgInsertAttrs is a 2 length array with Y1->N1 in it and cInsertAttrs = 2)
    // Final Run: R3 -> G2 -> Y1 -> N1 -> G1 -> B2

    // We'll need to know what the last valid column is for some calculations versus iEnd
    // because iEnd is specified to us as an inclusive index value.
    // Do the -1 math here now so we don't have to have -1s scattered all over this function.
    const size_t iLastBufferCol = cBufferWidth - 1;

    // If the insertion size is 1, do some pre-processing to
    // see if we can get this done quickly.
    if (newAttrs.size() == 1)
    {
        // Get the new color attribute we're trying to apply
        const TextAttribute NewAttr = newAttrs.at(0).GetAttributes();

        // If the existing run was only 1 element...
        // ...and the new color is the same as the old, we don't have to do anything and can exit quick.
        if (_list.size() == 1 && _list.at(0).GetAttributes() == NewAttr)
        {
            return S_OK;
        }
        // .. otherwise if we internally have a list of 2 or more and we're about to insert a single color
        // it's possible that we just walk left-to-right through the row and find a quick exit.
        else if (iStart > 0 && iStart == iEnd)
        {
            // First we try to find the run where the insertion happens, using lowerBound and upperBound to track
            // where we are curretly at.
            size_t lowerBound = 0;
            size_t upperBound = 0;
            for (size_t i = 0; i < _list.size(); i++)
            {
                upperBound += _list.at(i).GetLength();
                if (iStart >= lowerBound && iStart < upperBound)
                {
                    const auto curr = std::next(_list.begin(), i);

                    // The run that we try to insert into has the same color as the new one.
                    // e.g.
                    // AAAAABBBBBBBCCC
                    //       ^
                    // AAAAABBBBBBBCCC
                    //
                    // 'B' is the new color and '^' represents where iStart is. We don't have to
                    // do anything.
                    if (curr->GetAttributes() == NewAttr)
                    {
                        return S_OK;
                    }

                    // If the insertion happens at current run's lower boundary...
                    if (iStart == lowerBound)
                    {
                        const auto prev = std::prev(curr, 1);
                        // ... and the previous run has the same color as the new one, we can
                        // just adjust the counts in the existing two elements in our internal list.
                        // e.g.
                        // AAAAABBBBBBBCCC
                        //      ^
                        // AAAAAABBBBBBCCC
                        //
                        // Here 'A' is the new color.
                        if (NewAttr == prev->GetAttributes())
                        {
                            prev->IncrementLength();
                            curr->DecrementLength();

                            // If we just reduced the right half to zero, just erase it out of the list.
                            if (curr->GetLength() == 0)
                            {
                                _list.erase(curr);
                            }

                            return S_OK;
                        }
                    }
                }

                // Advance one run in the _list.
                lowerBound = upperBound;

                // The lowerBound is larger than iStart, which means we fail to find an early exit at the run
                // where the insertion happens. We can just break out.
                if (lowerBound > iStart)
                {
                    break;
                }
            }
        }
    }

    // If we're about to cover the entire existing run with a new one, we can also make an optimization.
    if (iStart == 0 && iEnd == iLastBufferCol)
    {
        // Just dump what we're given over what we have and call it a day.
        _list.assign(newAttrs.cbegin(), newAttrs.cend());

        return S_OK;
    }

    // In the worst case scenario, we will need a new run that is the length of
    // The existing run in memory + The new run in memory + 1.
    // This worst case occurs when we inject a new item in the middle of an existing run like so
    // Existing R3->B5->G2, Insertion Y2 starting at 5 (in the middle of the B5)
    // becomes R3->B2->Y2->B1->G2.
    // The original run was 3 long. The insertion run was 1 long. We need 1 more for the
    // fact that an existing piece of the run was split in half (to hold the latter half).
    const size_t cNewRun = _list.size() + newAttrs.size() + 1;
    std::vector<TextAttributeRun> newRun;
    newRun.resize(cNewRun);

    // We will start analyzing from the beginning of our existing run.
    // Use some pointers to keep track of where we are in walking through our runs.

    // Get the existing run that we'll be updating/manipulating.
    const auto existingRun = _list.begin();
    auto pExistingRunPos = existingRun;
    const auto pExistingRunEnd = existingRun + _list.size();
    auto pInsertRunPos = newAttrs.begin();
    size_t cInsertRunRemaining = newAttrs.size();
    auto pNewRunPos = newRun.begin();
    size_t iExistingRunCoverage = 0;

    // Copy the existing run into the new buffer up to the "start index" where the new run will be injected.
    // If the new run starts at 0, we have nothing to copy from the beginning.
    if (iStart != 0)
    {
        // While we're less than the desired insertion position...
        while (iExistingRunCoverage < iStart)
        {
            // Add up how much length we can cover by copying an item from the existing run.
            iExistingRunCoverage += pExistingRunPos->GetLength();

            // Copy it to the new run buffer and advance both pointers.
            *pNewRunPos++ = *pExistingRunPos++;
        }

        // When we get to this point, we've copied full segments from the original existing run
        // into our new run buffer. We will have 1 or more full segments of color attributes and
        // we MIGHT have to cut the last copied segment's length back depending on where the inserted
        // attributes will fall in the final/new run.
        // Some examples:
        // - Starting with the original string R3 -> G5 -> B2
        // - 1. If the insertion is Y5 at start index 3
        //      We are trying to get a result/final/new run of R3 -> Y5 -> B2.
        //      We just copied R3 to the new destination buffer and we cang skip down and start inserting the new attrs.
        // - 2. If the insertion is Y3 at start index 5
        //      We are trying to get a result/final/new run of R3 -> G2 -> Y3 -> B2.
        //      We just copied R3 -> G5 to the new destination buffer with the code above.
        //      But the insertion is going to cut out some of the length of the G5.
        //      We need to fix this up below so it says G2 instead to leave room for the Y3 to fit in
        //      the new/final run.

        // Copying above advanced the pointer to an empty cell beyond what we copied.
        // Back up one cell so we can manipulate the final item we copied from the existing run to the new run.
        pNewRunPos--;

        // Fetch out the length so we can fix it up based on the below conditions.
        size_t length = pNewRunPos->GetLength();

        // If we've covered more cells already than the start of the attributes to be inserted...
        if (iExistingRunCoverage > iStart)
        {
            // ..then subtract some of the length of the final cell we copied.
            // We want to take remove the difference in distance between the cells we've covered in the new
            // run and the insertion point.
            // (This turns G5 into G2 from Example 2 just above)
            length -= (iExistingRunCoverage - iStart);
        }

        // Now we're still on that "last cell copied" into the new run.
        // If the color of that existing copied cell matches the color of the first segment
        // of the run we're about to insert, we can just increment the length to extend the coverage.
        if (pNewRunPos->GetAttributes() == pInsertRunPos->GetAttributes())
        {
            length += pInsertRunPos->GetLength();

            // Since the color matched, we have already "used up" part of the insert run
            // and can skip it in our big "memcopy" step below that will copy the bulk of the insert run.
            cInsertRunRemaining--;
            pInsertRunPos++;
        }

        // We're done manipulating the length. Store it back.
        pNewRunPos->SetLength(length);

        // Now that we're done adjusting the last copied item, advance the pointer into a fresh/blank
        // part of the new run array.
        pNewRunPos++;
    }

    // Bulk copy the majority (or all, depending on circumstance) of the insert run into the final run buffer.
    std::copy_n(pInsertRunPos, cInsertRunRemaining, pNewRunPos);

    // Advance the new run pointer into the position just after everything we copied.
    pNewRunPos += cInsertRunRemaining;

    // We're technically done with the insert run now and have 0 remaining, but won't bother updating its pointers
    // and counts any further because we won't use them.

    // Now we need to move our pointer for the original existing run forward and update our counts
    // on how many cells we could have copied from the source before finishing off the new run.
    while (iExistingRunCoverage <= iEnd)
    {
        FAIL_FAST_IF(!(pExistingRunPos != pExistingRunEnd));
        iExistingRunCoverage += pExistingRunPos->GetLength();
        pExistingRunPos++;
    }

    // If we still have original existing run cells remaining, copy them into the final new run.
    if (pExistingRunPos != pExistingRunEnd || iExistingRunCoverage != (iEnd + 1))
    {
        // Back up one cell so we can inspect the most recent item copied into the new run for optimizations.
        pNewRunPos--;

        // We advanced the existing run pointer and its count to on or past the end of what the insertion run filled in.
        // If this ended up being past the end of what the insertion run covers, we have to account for the cells after
        // the insertion run but before the next piece of the original existing run.
        // The example in this case is if we had...
        // Existing Run = R3 -> G5 -> B2 -> X5
        // Insert Run = Y2 @ iStart = 7 and iEnd = 8
        // ... then at this point in time, our states would look like...
        // New Run so far = R3 -> G4 -> Y2
        // Existing Run Pointer is at X5
        // Existing run coverage count at 3 + 5 + 2 = 10.
        // However, in order to get the final desired New Run
        //   (which is R3 -> G4 -> Y2 -> B1 -> X5)
        //   we would need to grab a piece of that B2 we already skipped past.
        // iExistingRunCoverage = 10. iEnd = 8. iEnd+1 = 9. 10 > 9. So we skipped something.
        if (iExistingRunCoverage > (iEnd + 1))
        {
            // Back up the existing run pointer so we can grab the piece we skipped.
            pExistingRunPos--;

            // If the color matches what's already in our run, just increment the count value.
            // This case is slightly off from the example above. This case is for if the B2 above was actually Y2.
            // That Y2 from the existing run is the same color as the Y2 we just filled a few columns left in the final run
            // so we can just adjust the final run's column count instead of adding another segment here.
            if (pNewRunPos->GetAttributes() == pExistingRunPos->GetAttributes())
            {
                size_t length = pNewRunPos->GetLength();
                length += (iExistingRunCoverage - (iEnd + 1));
                pNewRunPos->SetLength(length);
            }
            else
            {
                // If the color didn't match, then we just need to copy the piece we skipped and adjust
                // its length for the discrepency in columns not yet covered by the final/new run.

                // Move forward to a blank spot in the new run
                pNewRunPos++;

                // Copy the existing run's color information to the new run
                pNewRunPos->SetAttributes(pExistingRunPos->GetAttributes());

                // Adjust the length of that copied color to cover only the reduced number of columns needed
                // now that some have been replaced by the insert run.
                pNewRunPos->SetLength(iExistingRunCoverage - (iEnd + 1));
            }

            // Now that we're done recovering a piece of the existing run we skipped, move the pointer forward again.
            pExistingRunPos++;
        }

        // OK. In this case, we didn't skip anything. The end of the insert run fell right at a boundary
        // in columns that was in the original existing run.
        // However, the next piece of the original existing run might happen to have the same color attribute
        // as the final piece of what we just copied.
        // As an example...
        // Existing Run = R3 -> G5 -> B2.
        // Insert Run = B5 @ iStart = 3 and iEnd = 7
        // New Run so far = R3 -> B5
        // New Run desired when done = R3 -> B7
        // Existing run pointer is on B2.
        // We want to merge the 2 from the B2 into the B5 so we get B7.
        else if (pNewRunPos->GetAttributes() == pExistingRunPos->GetAttributes())
        {
            // Add the value from the existing run into the current new run position.
            size_t length = pNewRunPos->GetLength();
            length += pExistingRunPos->GetLength();
            pNewRunPos->SetLength(length);

            // Advance the existing run position since we consumed its value and merged it in.
            pExistingRunPos++;
        }

        // OK. We're done inspecting the most recently copied cell for optimizations.
        pNewRunPos++;

        // Now bulk copy any segments left in the original existing run
        if (pExistingRunPos < pExistingRunEnd)
        {
            std::copy_n(pExistingRunPos, (pExistingRunEnd - pExistingRunPos), pNewRunPos);

            // Fix up the end pointer so we know where we are for counting how much of the new run's memory space we used.
            pNewRunPos += (pExistingRunEnd - pExistingRunPos);
        }
    }

    // OK, phew. We're done. Now we just need to free the existing run, store the new run in its place,
    // and update the count for the correct length of the new run now that we've filled it up.

    newRun.erase(pNewRunPos, newRun.end());
    _list.swap(newRun);

    return S_OK;
}

// Routine Description:
// - packs a vector of TextAttribute into a vector of TextAttrbuteRun
// Arguments:
// - attrs - text attributes to pack
// Return Value:
// - packed text attribute run
std::vector<TextAttributeRun> ATTR_ROW::PackAttrs(const std::vector<TextAttribute>& attrs)
{
    std::vector<TextAttributeRun> runs;
    if (attrs.empty())
    {
        return runs;
    }
    for (auto attr : attrs)
    {
        if (runs.empty() || runs.back().GetAttributes() != attr)
        {
            const TextAttributeRun run(1, attr);
            runs.push_back(run);
        }
        else
        {
            runs.back().SetLength(runs.back().GetLength() + 1);
        }
    }
    return runs;
}

ATTR_ROW::const_iterator ATTR_ROW::begin() const noexcept
{
    return AttrRowIterator(this);
}

ATTR_ROW::const_iterator ATTR_ROW::end() const noexcept
{
    return AttrRowIterator::CreateEndIterator(this);
}

ATTR_ROW::const_iterator ATTR_ROW::cbegin() const noexcept
{
    return AttrRowIterator(this);
}

ATTR_ROW::const_iterator ATTR_ROW::cend() const noexcept
{
    return AttrRowIterator::CreateEndIterator(this);
}

bool operator==(const ATTR_ROW& a, const ATTR_ROW& b) noexcept
{
    return (a._list.size() == b._list.size() &&
            a._list.data() == b._list.data() &&
            a._cchRowWidth == b._cchRowWidth);
}
