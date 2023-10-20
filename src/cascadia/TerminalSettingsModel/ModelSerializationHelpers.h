/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ModelSerializationHelpers.h

Abstract:
- Helper methods for serializing/de-serializing model data.

--*/

#pragma once

// Function Description:
// - Helper for converting a pair of comma separated, potentially absent integer values
//   into the corresponding left and right values. The leftValue and rightValue functions
//   will be called back with the associated parsed integer value, assuming it's present.
//   (100, 100): leftValue and rightValue functions both called back with 100.
//   (100, ), (100, abc): leftValue function called back with 100
//   (, 100), (abc, 100): rightValue function called back with 100
//   (,): no function called back
//   (100, 100, 100): we only read the first two values, this is equivalent to (100, 100)
// Arguments:
// - string: The string to parse
// - leftValue: Function called back with the value before the comma
// - rightValue: Function called back with the value after the comma
_TIL_INLINEPREFIX void ParseCommaSeparatedPair(const std::string& string,
                                               std::function<void(int32_t)> leftValue,
                                               std::function<void(int32_t)> rightValue)
{
    static constexpr auto singleCharDelim = ',';
    std::stringstream tokenStream(string);
    std::string token;
    uint8_t index = 0;

    // Get values till we run out of delimiter separated values in the stream
    // or we hit max number of allowable values (= 2)
    // Non-numeral values or empty string will be caught as exception and we do not assign them
    for (; std::getline(tokenStream, token, singleCharDelim) && (index < 2); index++)
    {
        try
        {
            int32_t value = std::stol(token);
            if (index == 0)
            {
                leftValue(value);
            }

            if (index == 1)
            {
                rightValue(value);
            }
        }
        catch (...)
        {
            // Do nothing
        }
    }
}

// See: ParseCommaSeparatedPair
_TIL_INLINEPREFIX ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition LaunchPositionFromString(const std::string& string)
{
    ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition initialPosition;
    ParseCommaSeparatedPair(
        string,
        [&initialPosition](int32_t left) { initialPosition.X = left; },
        [&initialPosition](int32_t right) { initialPosition.Y = right; });
    return initialPosition;
}

// See: ParseCommaSeparatedPair
_TIL_INLINEPREFIX ::til::size SizeFromString(const std::string& string)
{
    til::size size{};
    ParseCommaSeparatedPair(
        string,
        [&size](int32_t left) { size.width = left; },
        [&size](int32_t right) { size.height = right; });
    return size;
}
