// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// The at function declares that you've already sufficiently checked that your array access
// is in range before retrieving an item inside it at an offset.
// This is to save double/triple/quadruple testing in circumstances where you are already
// pivoting on the length of a set and now want to pull elements out of it by offset
// without checking again.
// gsl::at will do the check again. As will .at(). And using [] will have a warning in audit.

namespace til
{
    // Routine Description:
    // - Takes a reference to a sequence of constant data and returns the item at the given offset.
    //   NOTE: The function relies on a sufficient check that the access is in range.
    // Arguments:
    // - sequence - Reference to constant data in a range of data accessible using the subscript operator.
    //              Such like constant STL strings, containers, bitsets, random-access iterators, or pointers.
    // - index    - Offset of the element to be returned.
    // Return Value:
    // - Element in the sequence at the offset given by the index parameter.
    template<class T, class U>
    constexpr auto at(const T& sequence, const U index) -> typename std::enable_if<std::is_integral<U>::value, decltype(sequence[0])>::type
    {
#pragma warning(push)
#pragma warning(suppress : 26481 26482 26446) // Suppress checks for pointer arithmetik, indexing with constant expressions, and subscript operator.
        return sequence[index];
#pragma warning(pop)
    }

    // Routine Description:
    // - Takes a reference to a sequence of data and returns the item at the given offset.
    //   NOTE: The function relies on a sufficient check that the access is in range.
    // Arguments:
    // - sequence - Reference to constant data in a range of data accessible using the subscript operator.
    //              Such like STL strings, containers, bitsets, random-access iterators, or pointers.
    // - index    - Offset of the element to be returned.
    // Return Value:
    // - Element in the sequence at the offset given by the index parameter.
    template<class T, class U>
    constexpr auto at(T& sequence, const U index) -> typename std::enable_if<std::is_integral<U>::value, decltype(sequence[0])>::type
    {
#pragma warning(push)
#pragma warning(suppress : 26481 26482 26446) // Suppress checks for pointer arithmetik, indexing with constant expressions, and subscript operator.
        return sequence[index];
#pragma warning(pop)
    }
}
