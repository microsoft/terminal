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
// - Helper for converting a launch position string into
//   2 coordinate values. We allow users to only provide one coordinate,
//   thus, we use comma as the separator:
//   (100, 100): standard input string
//   (, 100), (100, ): if a value is missing, we set this value as a default
//   (,): both x and y are set to default
//   (abc, 100): if a value is not valid, we treat it as default
//   (100, 100, 100): we only read the first two values, this is equivalent to (100, 100)
// Arguments:
// - The string to convert to a LaunchPosition
// Return value:
// - The de-serialized LaunchPosition, or one representing the default position if the
//   string is invalid.
_TIL_INLINEPREFIX ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition LaunchPositionFromString(const std::string& string)
{
    ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition ret;
    static constexpr auto singleCharDelim = ',';
    std::stringstream tokenStream(string);
    std::string token;
    uint8_t initialPosIndex = 0;

    // Get initial position values till we run out of delimiter separated values in the stream
    // or we hit max number of allowable values (= 2)
    // Non-numeral values or empty string will be caught as exception and we do not assign them
    for (; std::getline(tokenStream, token, singleCharDelim) && (initialPosIndex < 2); initialPosIndex++)
    {
        try
        {
            int64_t position = std::stol(token);
            if (initialPosIndex == 0)
            {
                ret.X = position;
            }

            if (initialPosIndex == 1)
            {
                ret.Y = position;
            }
        }
        catch (...)
        {
            // Do nothing
        }
    }
    return ret;
}
