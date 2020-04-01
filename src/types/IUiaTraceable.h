/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaTraceable.hpp

Abstract:
- This module is used for assigning and retrieving IDs to Uia objects

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

    protected:
        // used to debug objects passed back and forth
        // between the provider and the client
        IdType _id{};
    };
}
