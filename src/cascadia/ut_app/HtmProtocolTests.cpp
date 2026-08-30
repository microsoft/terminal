// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../TerminalApp/HtmProtocol.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace Microsoft::Terminal::Htm;

namespace TerminalAppUnitTests
{
    class HtmProtocolTests
    {
        TEST_CLASS(HtmProtocolTests);

        TEST_METHOD(EncodeLengthRoundTrip)
        {
            const auto encoded = EncodeLength(1);
            VERIFY_ARE_EQUAL(8u, encoded.size());
            VERIFY_ARE_EQUAL(1, DecodeLength(encoded));
            VERIFY_ARE_EQUAL(128, DecodeLength(EncodeLength(128)));
        }

        TEST_METHOD(SessionEndIsOneByte)
        {
            std::string buffer;
            buffer.push_back(SessionEnd);
            buffer.append("leftover");
            const auto [packets, rest] = ParsePackets(buffer);
            VERIFY_ARE_EQUAL(1u, packets.size());
            VERIFY_ARE_EQUAL(SessionEnd, packets[0].header);
            VERIFY_ARE_EQUAL("leftover", rest);
        }

        TEST_METHOD(ParseFramedPacket)
        {
            const auto framed = FramePacket(InitState, R"({"tabs":{}})");
            const auto [packets, rest] = ParsePackets(framed);
            VERIFY_ARE_EQUAL(1u, packets.size());
            VERIFY_ARE_EQUAL(InitState, packets[0].header);
            VERIFY_ARE_EQUAL(R"({"tabs":{}})", packets[0].payload);
            VERIFY_IS_TRUE(rest.empty());
        }

        TEST_METHOD(PartialPacketStaysInBuffer)
        {
            auto framed = FramePacket(DebugLog, "abcd");
            framed.resize(5); // header + 4 of 8 length chars
            const auto [packets, rest] = ParsePackets(framed);
            VERIFY_IS_TRUE(packets.empty());
            VERIFY_ARE_EQUAL(framed, rest);
        }

        TEST_METHOD(ConsumeInitAcrossChunks)
        {
            const auto first = ConsumeInitPayload("", "hello\x1b[#");
            VERIFY_IS_FALSE(first.matched);
            VERIFY_ARE_EQUAL("hello", first.prefix);
            VERIFY_ARE_EQUAL("\x1b[#", first.pending);

            const auto second = ConsumeInitPayload(first.pending, "##qREST");
            VERIFY_IS_TRUE(second.matched);
            VERIFY_ARE_EQUAL("REST", second.remainder);
        }

        TEST_METHOD(InsertKeysFrameContainsUuidAndPayload)
        {
            const auto pane = "12345678-1234-1234-1234-1234567890ab";
            VERIFY_ARE_EQUAL(UuidLength, pane.size());
            const auto packet = FrameInsertKeys(pane, "hi");
            const auto [packets, rest] = ParsePackets(packet);
            VERIFY_ARE_EQUAL(1u, packets.size());
            VERIFY_ARE_EQUAL(InsertKeys, packets[0].header);
            VERIFY_ARE_EQUAL(pane, packets[0].payload.substr(0, UuidLength));
            VERIFY_ARE_EQUAL("hi", Base64Decode(packets[0].payload.substr(UuidLength)));
        }
    };
}
