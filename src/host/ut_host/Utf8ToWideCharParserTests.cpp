// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "utf8ToWideCharParser.hpp"

#define IsBitSet WI_IsFlagSet

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace std;

class Utf8ToWideCharParserTests
{
    static const unsigned int utf8CodePage = 65001;
    static const unsigned int USACodePage = 1252;

    TEST_CLASS(Utf8ToWideCharParserTests);

    TEST_METHOD(ConvertsAsciiTest)
    {
        Log::Comment(L"Testing that ASCII chars are correctly converted to wide chars");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        // ascii "hello"
        const unsigned char hello[5] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
        const unsigned char wideHello[10] = { 0x48, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x6f, 0x00 };
        unsigned int count = 5;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };

        VERIFY_SUCCEEDED(parser.Parse(hello, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)5);
        VERIFY_ARE_EQUAL(generated, (unsigned int)5);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < ARRAYSIZE(wideHello); ++i)
        {
            VERIFY_ARE_EQUAL(wideHello[i], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(ConvertSimpleUtf8Test)
    {
        Log::Comment(L"Testing that a simple UTF8 sequence can be converted");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        // U+3059, U+3057 (hiragana sushi)
        const unsigned char sushi[6] = { 0xe3, 0x81, 0x99, 0xe3, 0x81, 0x97 };
        const unsigned char wideSushi[4] = { 0x59, 0x30, 0x57, 0x30 };
        unsigned int count = 6;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };

        VERIFY_SUCCEEDED(parser.Parse(sushi, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)6);
        VERIFY_ARE_EQUAL(generated, (unsigned int)2);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < ARRAYSIZE(wideSushi); ++i)
        {
            VERIFY_ARE_EQUAL(wideSushi[i], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(WaitsForAdditionalInputAfterPartialSequenceTest)
    {
        Log::Comment(L"Testing that nothing is returned when parsing a partial sequence until the sequence is complete");
        // U+3057 (hiragana shi)
        unsigned char shi[3] = { 0xe3, 0x81, 0x97 };
        unsigned char wideShi[2] = { 0x57, 0x30 };
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        unsigned int count = 1;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };

        for (int i = 0; i < 2; ++i)
        {
            VERIFY_SUCCEEDED(parser.Parse(shi + i, count, consumed, output, generated));
            VERIFY_ARE_EQUAL(consumed, (unsigned int)1);
            VERIFY_ARE_EQUAL(generated, (unsigned int)0);
            VERIFY_ARE_EQUAL(output.get(), nullptr);
            count = 1;
        }

        VERIFY_SUCCEEDED(parser.Parse(shi + 2, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)1);
        VERIFY_ARE_EQUAL(generated, (unsigned int)1);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < ARRAYSIZE(wideShi); ++i)
        {
            VERIFY_ARE_EQUAL(wideShi[i], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(ReturnsInitialPartOfSequenceThatEndsWithPartialTest)
    {
        Log::Comment(L"Testing that a valid portion of a sequence is returned when it ends with a partial sequence");
        // U+3059, U+3057 (hiragana sushi)
        const unsigned char sushi[6] = { 0xe3, 0x81, 0x99, 0xe3, 0x81, 0x97 };
        const unsigned char wideSushi[4] = { 0x59, 0x30, 0x57, 0x30 };
        unsigned int count = 4;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };
        auto parser = Utf8ToWideCharParser{ utf8CodePage };

        VERIFY_SUCCEEDED(parser.Parse(sushi, count, consumed, output, generated));
        // check that we got the first wide char back
        VERIFY_ARE_EQUAL(consumed, (unsigned int)4);
        VERIFY_ARE_EQUAL(generated, (unsigned int)1);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < 2; ++i)
        {
            VERIFY_ARE_EQUAL(wideSushi[i], pReturnedBytes[i]);
        }

        // add byte 2 of 3 to parser
        count = 1;
        consumed = 0;
        generated = 0;
        output.reset(nullptr);
        VERIFY_SUCCEEDED(parser.Parse(sushi + 4, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)1);
        VERIFY_ARE_EQUAL(generated, (unsigned int)0);
        VERIFY_ARE_EQUAL(output.get(), nullptr);

        // add last byte
        count = 1;
        consumed = 0;
        generated = 0;
        output.reset(nullptr);
        VERIFY_SUCCEEDED(parser.Parse(sushi + 5, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)1);
        VERIFY_ARE_EQUAL(generated, (unsigned int)1);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < 2; ++i)
        {
            VERIFY_ARE_EQUAL(wideSushi[i + 2], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(MergesMultiplePartialSequencesTest)
    {
        Log::Comment(L"Testing that partial sequences sent individually will be merged together");

        // clang-format off
        // (hiragana doomo arigatoo)
        const unsigned char doomoArigatoo[24] = {
            0xe3, 0x81, 0xa9, // U+3069
            0xe3, 0x81, 0x86, // U+3046
            0xe3, 0x82, 0x82, // U+3082
            0xe3, 0x81, 0x82, // U+3042
            0xe3, 0x82, 0x8a, // U+308A
            0xe3, 0x81, 0x8c, // U+304C
            0xe3, 0x81, 0xa8, // U+3068
            0xe3, 0x81, 0x86  // U+3046
        };
        const unsigned char wideDoomoArigatoo[16] = {
            0x69, 0x30,
            0x46, 0x30,
            0x82, 0x30,
            0x42, 0x30,
            0x8a, 0x30,
            0x4c, 0x30,
            0x68, 0x30,
            0x46, 0x30
        };
        // clang-format on

        // send first 4 bytes
        unsigned int count = 4;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };
        auto parser = Utf8ToWideCharParser{ utf8CodePage };

        VERIFY_SUCCEEDED(parser.Parse(doomoArigatoo, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)4);
        VERIFY_ARE_EQUAL(generated, (unsigned int)1);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < 2; ++i)
        {
            VERIFY_ARE_EQUAL(wideDoomoArigatoo[i], pReturnedBytes[i]);
        }

        // send next 16 bytes
        count = 16;
        consumed = 0;
        generated = 0;
        output.reset(nullptr);
        VERIFY_SUCCEEDED(parser.Parse(doomoArigatoo + 4, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)16);
        VERIFY_ARE_EQUAL(generated, (unsigned int)5);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < 10; ++i)
        {
            VERIFY_ARE_EQUAL(wideDoomoArigatoo[i + 2], pReturnedBytes[i]);
        }

        // send last 4 bytes
        count = 4;
        consumed = 0;
        generated = 0;
        output.reset(nullptr);
        VERIFY_SUCCEEDED(parser.Parse(doomoArigatoo + 20, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)4);
        VERIFY_ARE_EQUAL(generated, (unsigned int)2);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < 4; ++i)
        {
            VERIFY_ARE_EQUAL(wideDoomoArigatoo[i + 12], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(RemovesInvalidSequencesTest)
    {
        Log::Comment(L"Testing that invalid sequences are removed and don't stop the parsing of the rest");

        // clang-format off
        // hiragana sushi with junk between japanese characters
        const unsigned char sushi[9] = {
            0xe3, 0x81, 0x99, // U+3059
            0x80, 0x81, 0x82, // junk continuation bytes
            0xe3, 0x81, 0x97  // U+3057
        };
        // clang-format on

        const unsigned char wideSushi[4] = { 0x59, 0x30, 0x57, 0x30 };
        unsigned int count = 9;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };
        auto parser = Utf8ToWideCharParser{ utf8CodePage };

        VERIFY_SUCCEEDED(parser.Parse(sushi, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(consumed, (unsigned int)9);
        VERIFY_ARE_EQUAL(generated, (unsigned int)2);
        VERIFY_ARE_NOT_EQUAL(output.get(), nullptr);

        unsigned char* pReturnedBytes = reinterpret_cast<unsigned char*>(output.get());
        for (int i = 0; i < ARRAYSIZE(wideSushi); ++i)
        {
            VERIFY_ARE_EQUAL(wideSushi[i], pReturnedBytes[i]);
        }
    }

    TEST_METHOD(NonMinimalFormTest)
    {
        Log::Comment(L"Testing that non-minimal forms of a character are tolerated don't stop the rest");

        // clang-format off

        // Test data
        const unsigned char data[] = {
            0x60, 0x12, 0x08, 0x7f, // single byte points
            0xc0, 0x80, // U+0000 as a 2-byte sequence (non-minimal)
            0x41, 0x48, 0x06, 0x55, // more single byte points
            0xe0, 0x80, 0x80, // U+0000 as a 3-byte sequence (non-minimal)
            0x18, 0x77, 0x40, 0x31, // more single byte points
            0xf0, 0x80, 0x80, 0x80, // U+0000 as a 4-byte sequence (non-minimal)
            0x59, 0x1f, 0x68, 0x20 // more single byte points
        };

        // Expected conversion
        const wchar_t wideData[] = {
            0x0060, 0x0012, 0x0008, 0x007f,
            0xfffd, 0xfffd, // The number of replacements per invalid sequence is not intended to be load-bearing
            0x0041, 0x0048, 0x0006, 0x0055,
            0xfffd, 0xfffd, // It is just representative of what it looked like when fixing this for GH#3380
            0x0018, 0x0077, 0x0040, 0x0031,
            0xfffd, 0xfffd, 0xfffd, // Change if necessary when completing GH#3378
            0x0059, 0x001f, 0x0068, 0x0020
        };

        // clang-format on

        const unsigned int count = gsl::narrow_cast<unsigned int>(ARRAYSIZE(data));
        const unsigned int wideCount = gsl::narrow_cast<unsigned int>(ARRAYSIZE(wideData));
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };
        auto parser = Utf8ToWideCharParser{ utf8CodePage };

        VERIFY_SUCCEEDED(parser.Parse(data, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(count, consumed);
        VERIFY_ARE_EQUAL(wideCount, generated);
        VERIFY_IS_NOT_NULL(output.get());

        const auto expected = WEX::Common::String(wideData, wideCount);
        const auto actual = WEX::Common::String(output.get(), generated);
        VERIFY_ARE_EQUAL(expected, actual);
    }

    TEST_METHOD(PartialBytesAreDroppedOnCodePageChangeTest)
    {
        Log::Comment(L"Testing that a saved partial sequence is cleared when the codepage changes");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        // 2 bytes of a 4 byte sequence
        const unsigned int inputSize = 2;
        const unsigned char partialSequence[inputSize] = { 0xF0, 0x80 };
        unsigned int count = inputSize;
        unsigned int consumed = 0;
        unsigned int generated = 0;
        unique_ptr<wchar_t[]> output{ nullptr };
        VERIFY_SUCCEEDED(parser.Parse(partialSequence, count, consumed, output, generated));
        VERIFY_ARE_EQUAL(parser._currentState, Utf8ToWideCharParser::_State::BeginPartialParse);
        VERIFY_ARE_EQUAL(parser._bytesStored, inputSize);
        // set the codepage to the same one it currently is, ensure
        // that nothing changes
        parser.SetCodePage(utf8CodePage);
        VERIFY_ARE_EQUAL(parser._currentState, Utf8ToWideCharParser::_State::BeginPartialParse);
        VERIFY_ARE_EQUAL(parser._bytesStored, inputSize);
        // change to a different codepage, ensure parser is reset
        parser.SetCodePage(USACodePage);
        VERIFY_ARE_EQUAL(parser._currentState, Utf8ToWideCharParser::_State::Ready);
        VERIFY_ARE_EQUAL(parser._bytesStored, (unsigned int)0);
    }

    TEST_METHOD(_IsLeadByteTest)
    {
        Log::Comment(L"Testing that _IsLeadByte properly differentiates correct from incorrect sequences");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        VERIFY_IS_TRUE(parser._IsLeadByte(0xC0)); // 2 byte sequence
        VERIFY_IS_TRUE(parser._IsLeadByte(0xE0)); // 3 byte sequence
        VERIFY_IS_TRUE(parser._IsLeadByte(0xF0)); // 4 byte sequence
        VERIFY_IS_FALSE(parser._IsLeadByte(0x00)); // ASCII char NUL
        VERIFY_IS_FALSE(parser._IsLeadByte(0x80)); // continuation byte
        VERIFY_IS_FALSE(parser._IsLeadByte(0x83)); // continuation byte
        VERIFY_IS_FALSE(parser._IsLeadByte(0x7E)); // ASCII char '~'
        VERIFY_IS_FALSE(parser._IsLeadByte(0x21)); // ASCII char '!'
        VERIFY_IS_FALSE(parser._IsLeadByte(0xF8)); // invalid 5 byte sequence
        VERIFY_IS_FALSE(parser._IsLeadByte(0xFC)); // invalid 6 byte sequence
        VERIFY_IS_FALSE(parser._IsLeadByte(0xFE)); // invalid 7 byte sequence
        VERIFY_IS_FALSE(parser._IsLeadByte(0xFF)); // all 1's
    }

    TEST_METHOD(_IsContinuationByteTest)
    {
        Log::Comment(L"Testing that _IsContinuationByte properly differentiates correct from incorrect sequences");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        for (BYTE i = 0x00; i < 0xFF; ++i)
        {
            if (IsBitSet(i, 0x80) && !IsBitSet(i, 0x40))
            {
                VERIFY_IS_TRUE(parser._IsContinuationByte(i), NoThrowString().Format(L"Byte is 0x%02x", i));
            }
            else
            {
                VERIFY_IS_FALSE(parser._IsContinuationByte(i), NoThrowString().Format(L"Byte is 0x%02x", i));
            }
        }
        VERIFY_IS_FALSE(parser._IsContinuationByte(0xFF));
    }

    TEST_METHOD(_IsAsciiByteTest)
    {
        Log::Comment(L"Testing that _IsAsciiByte properly differentiates correct from incorrect sequences");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        for (BYTE i = 0x00; i < 0x80; ++i)
        {
            VERIFY_IS_TRUE(parser._IsAsciiByte(i), NoThrowString().Format(L"Byte is 0x%02x", i));
        }
        for (BYTE i = 0xFF; i > 0x7F; --i)
        {
            VERIFY_IS_FALSE(parser._IsAsciiByte(i), NoThrowString().Format(L"Byte is 0x%02x", i));
        }
    }

    TEST_METHOD(_Utf8SequenceSizeTest)
    {
        Log::Comment(L"Testing that _Utf8SequenceSize correctly counts the number of MSB 1's");
        auto parser = Utf8ToWideCharParser{ utf8CodePage };
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0x00), (unsigned int)0);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0x80), (unsigned int)1);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xC2), (unsigned int)2);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xE3), (unsigned int)3);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xF0), (unsigned int)4);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xF3), (unsigned int)4);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xF8), (unsigned int)5);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xFC), (unsigned int)6);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xFD), (unsigned int)6);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xFE), (unsigned int)7);
        VERIFY_ARE_EQUAL(parser._Utf8SequenceSize(0xFF), (unsigned int)8);
    }
};
