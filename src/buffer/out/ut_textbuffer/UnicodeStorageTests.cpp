// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../UnicodeStorage.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class UnicodeStorageTests
{
    TEST_CLASS(UnicodeStorageTests);

    TEST_METHOD(CanOverwriteEmoji)
    {
        UnicodeStorage storage;
        const COORD coord{ 1, 3 };
        const std::vector<wchar_t> newMoon{ 0xD83C, 0xDF11 };
        const std::vector<wchar_t> fullMoon{ 0xD83C, 0xDF15 };

        // store initial glyph
        storage.StoreGlyph(coord, newMoon);

        // verify it was stored
        auto findIt = storage._map.find(coord);
        VERIFY_ARE_NOT_EQUAL(findIt, storage._map.end());
        const std::vector<wchar_t>& newMoonGlyph = findIt->second;
        VERIFY_ARE_EQUAL(newMoonGlyph.size(), newMoon.size());
        for (size_t i = 0; i < newMoon.size(); ++i)
        {
            VERIFY_ARE_EQUAL(newMoonGlyph.at(i), newMoon.at(i));
        }

        // overwrite it
        storage.StoreGlyph(coord, fullMoon);

        // verify the glyph was overwritten
        findIt = storage._map.find(coord);
        VERIFY_ARE_NOT_EQUAL(findIt, storage._map.end());
        const std::vector<wchar_t>& fullMoonGlyph = findIt->second;
        VERIFY_ARE_EQUAL(fullMoonGlyph.size(), fullMoon.size());
        for (size_t i = 0; i < fullMoon.size(); ++i)
        {
            VERIFY_ARE_EQUAL(fullMoonGlyph.at(i), fullMoon.at(i));
        }
    }
};
