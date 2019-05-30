// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\inc\utils.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Console::Utils;

class UuidTests
{
    // {AD56DE9E-5167-41B6-80EB-FB19F7927D1A}
    static constexpr GUID TEST_NAMESPACE_GUID{ 0xad56de9e, 0x5167, 0x41b6, { 0x80, 0xeb, 0xfb, 0x19, 0xf7, 0x92, 0x7d, 0x1a } };

    TEST_CLASS(UuidTests);

    TEST_METHOD(TestV5UuidU8String)
    {
        const GUID uuidExpected{ 0x8b9d4336, 0x0c82, 0x54c4, { 0xb3, 0x15, 0xf1, 0xd2, 0xd2, 0x7e, 0xc6, 0xda } };

        std::string name{ "testing" };
        auto uuidActual = CreateV5Uuid(TEST_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(name)));

        VERIFY_ARE_EQUAL(uuidExpected, uuidActual);
    }

    TEST_METHOD(TestV5UuidU16String)
    {
        const GUID uuidExpected{ 0xe04fb1f7, 0x739d, 0x5d63, { 0xbb, 0x18, 0xe0, 0xea, 0x00, 0xb1, 0x9e, 0xe8 } };

        // This'll come out in little endian; the reference GUID was generated as such.
        std::wstring name{ L"testing" };
        auto uuidActual = CreateV5Uuid(TEST_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(name)));

        VERIFY_ARE_EQUAL(uuidExpected, uuidActual);
    }
};
