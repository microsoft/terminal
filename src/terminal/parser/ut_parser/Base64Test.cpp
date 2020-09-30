// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "base64.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class Base64Test;
        };
    };
};

using namespace Microsoft::Console::VirtualTerminal;

class Microsoft::Console::VirtualTerminal::Base64Test
{
    TEST_CLASS(Base64Test);

    TEST_METHOD(TestBase64Encode)
    {
        VERIFY_ARE_EQUAL(L"Zm9v", Base64::s_Encode(L"foo"));
        VERIFY_ARE_EQUAL(L"Zm9vYg==", Base64::s_Encode(L"foob"));
        VERIFY_ARE_EQUAL(L"Zm9vYmE=", Base64::s_Encode(L"fooba"));
        VERIFY_ARE_EQUAL(L"Zm9vYmFy", Base64::s_Encode(L"foobar"));
        VERIFY_ARE_EQUAL(L"Zm9vYmFyDQo=", Base64::s_Encode(L"foobar\r\n"));
    }

    TEST_METHOD(TestBase64Decode)
    {
        std::wstring result;
        bool success;

        success = Base64::s_Decode(L"Zm9v", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foo", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9vYg==", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foob", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9vYmE=", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"fooba", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9vYmFy", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foobar", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9vYmFyDQo=", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foobar\r\n", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9v\rYmFy", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foobar", result);

        result = L"";
        success = Base64::s_Decode(L"Zm9v\r\nYmFy\n", result);
        VERIFY_ARE_EQUAL(true, success);
        VERIFY_ARE_EQUAL(L"foobar", result);

        success = Base64::s_Decode(L"Z", result);
        VERIFY_ARE_EQUAL(false, success);

        success = Base64::s_Decode(L"Zm9vYg", result);
        VERIFY_ARE_EQUAL(false, success);

        success = Base64::s_Decode(L"Zm9vYg=", result);
        VERIFY_ARE_EQUAL(false, success);
    }
};
