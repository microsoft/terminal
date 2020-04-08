/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaTraceable.hpp

Abstract:
- This module is used for assigning and retrieving IDs to UIA objects

Author(s):
- Carlos Zamora (cazamor)     Apr 2020
--*/

#pragma once

namespace Microsoft::Console::Types
{
    typedef unsigned long long IdType;
    constexpr IdType InvalidId = 0;

    class IUiaTraceable
    {
    public:
        const IdType GetId() const noexcept
        {
            return _id;
        }

        // Routine Description:
        // - assigns an ID to the IUiaTraceable object if it doesn't have one
        // Arguments:
        // - id - the id value that we are trying to assign
        // Return Value:
        // - true if the assignment was successful, false otherwise (it already has an id).
        bool AssignId(IdType id) noexcept
        {
            if (_id == InvalidId)
            {
                _id = id;
                return true;
            }
            else
            {
                return false;
            }
        }

    private:
        // used to debug objects passed back and forth
        // between the provider and the client
        IdType _id{};
    };
}
