// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Zlib.hpp"

namespace Zlib
{
    namespace
    {
        // A canonical Huffman decoding table built from code lengths.
        struct Huffman
        {
            uint16_t counts[16]{};
            uint16_t symbols[288]{};
        };

        bool BuildHuffman(Huffman& table, const uint8_t* lengths, const int symbolCount)
        {
            for (auto& count : table.counts)
            {
                count = 0;
            }
            for (int i = 0; i < symbolCount; ++i)
            {
                ++table.counts[lengths[i]];
            }
            if (table.counts[0] == symbolCount)
            {
                return true; // no codes at all; legal for the distance tree
            }

            // Reject over-subscribed length sets.
            int available = 1;
            for (int length = 1; length < 16; ++length)
            {
                available <<= 1;
                available -= table.counts[length];
                if (available < 0)
                {
                    return false;
                }
            }

            // Offsets into the symbol table per code length.
            uint16_t offsets[16]{};
            for (int length = 1; length < 15; ++length)
            {
                offsets[length + 1] = offsets[length] + table.counts[length];
            }
            for (int i = 0; i < symbolCount; ++i)
            {
                if (lengths[i])
                {
                    table.symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
                }
            }
            return true;
        }

        struct Inflater
        {
            const uint8_t* data;
            size_t size;
            size_t bytePos = 0;
            uint32_t bitBuffer = 0;
            int bitCount = 0;
            std::vector<uint8_t> out;
            size_t maxOutput;
            bool failed = false;

            bool ReceiveBits(const int count, uint32_t& value)
            {
                while (bitCount < count)
                {
                    if (bytePos >= size)
                    {
                        return false;
                    }
                    bitBuffer |= static_cast<uint32_t>(data[bytePos++]) << bitCount;
                    bitCount += 8;
                }
                value = bitBuffer & ((1u << count) - 1);
                bitBuffer >>= count;
                bitCount -= count;
                return true;
            }

            int PeekBit()
            {
                while (bitCount < 1)
                {
                    if (bytePos >= size)
                    {
                        return -1;
                    }
                    bitBuffer |= static_cast<uint32_t>(data[bytePos++]) << bitCount;
                    bitCount += 8;
                }
                return static_cast<int>(bitBuffer & 1);
            }

            void DropBit()
            {
                bitBuffer >>= 1;
                --bitCount;
            }

            // Decodes one symbol, most-significant code bit first.
            int Decode(const Huffman& table)
            {
                int code = 0;
                int first = 0;
                int index = 0;
                for (int length = 1; length < 16; ++length)
                {
                    const auto bit = PeekBit();
                    if (bit < 0)
                    {
                        return -1;
                    }
                    DropBit();
                    code |= bit;
                    const auto count = table.counts[length];
                    if (code - first < count)
                    {
                        return table.symbols[index + (code - first)];
                    }
                    index += count;
                    first = (first + count) << 1;
                    code <<= 1;
                }
                return -1;
            }

            bool Emit(const uint8_t byte)
            {
                if (out.size() >= maxOutput)
                {
                    return false;
                }
                out.push_back(byte);
                return true;
            }

            bool CopyMatch(const int length, const int distance)
            {
                if (distance <= 0 || distance > static_cast<int>(out.size()))
                {
                    return false;
                }
                auto source = out.size() - static_cast<size_t>(distance);
                for (int i = 0; i < length; ++i)
                {
                    if (!Emit(out[source++]))
                    {
                        return false;
                    }
                }
                return true;
            }
        };

        constexpr int LengthBases[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
        constexpr int LengthExtras[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
        constexpr int DistanceBases[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
        constexpr int DistanceExtras[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
        constexpr int CodeLengthOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

        bool InflateBlock(Inflater& s, const Huffman& literals, const Huffman& distances)
        {
            for (;;)
            {
                const auto symbol = s.Decode(literals);
                if (symbol < 0)
                {
                    return false;
                }
                if (symbol < 256)
                {
                    if (!s.Emit(static_cast<uint8_t>(symbol)))
                    {
                        return false;
                    }
                }
                else if (symbol == 256)
                {
                    return true; // end of block
                }
                else if (symbol <= 285)
                {
                    const auto index = symbol - 257;
                    uint32_t extra = 0;
                    if (LengthExtras[index] && !s.ReceiveBits(LengthExtras[index], extra))
                    {
                        return false;
                    }
                    const auto length = LengthBases[index] + static_cast<int>(extra);

                    const auto distanceSymbol = s.Decode(distances);
                    if (distanceSymbol < 0 || distanceSymbol > 29)
                    {
                        return false;
                    }
                    extra = 0;
                    if (DistanceExtras[distanceSymbol] && !s.ReceiveBits(DistanceExtras[distanceSymbol], extra))
                    {
                        return false;
                    }
                    const auto distance = DistanceBases[distanceSymbol] + static_cast<int>(extra);
                    if (!s.CopyMatch(length, distance))
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }

        bool InflateRaw(Inflater& s)
        {
            Huffman literals;
            Huffman distances;

            for (;;)
            {
                const auto finalBit = s.PeekBit();
                if (finalBit < 0)
                {
                    return false;
                }
                s.DropBit();
                uint32_t type = 0;
                if (!s.ReceiveBits(2, type))
                {
                    return false;
                }

                if (type == 0)
                {
                    // Stored: realign to the byte boundary, copy LEN bytes.
                    s.bitBuffer = 0;
                    s.bitCount = 0;
                    if (s.bytePos + 4 > s.size)
                    {
                        return false;
                    }
                    const auto length = s.data[s.bytePos] | (s.data[s.bytePos + 1] << 8);
                    const auto inverse = s.data[s.bytePos + 2] | (s.data[s.bytePos + 3] << 8);
                    s.bytePos += 4;
                    if (length != (~inverse & 0xFFFF) || s.bytePos + length > s.size)
                    {
                        return false;
                    }
                    for (int i = 0; i < length; ++i)
                    {
                        if (!s.Emit(s.data[s.bytePos++]))
                        {
                            return false;
                        }
                    }
                }
                else if (type == 1)
                {
                    // Fixed Huffman tables.
                    uint8_t lengths[288];
                    int i = 0;
                    for (; i < 144; ++i)
                    {
                        lengths[i] = 8;
                    }
                    for (; i < 256; ++i)
                    {
                        lengths[i] = 9;
                    }
                    for (; i < 280; ++i)
                    {
                        lengths[i] = 7;
                    }
                    for (; i < 288; ++i)
                    {
                        lengths[i] = 8;
                    }
                    uint8_t distanceLengths[30];
                    for (i = 0; i < 30; ++i)
                    {
                        distanceLengths[i] = 5;
                    }
                    if (!BuildHuffman(literals, lengths, 288) || !BuildHuffman(distances, distanceLengths, 30))
                    {
                        return false;
                    }
                    if (!InflateBlock(s, literals, distances))
                    {
                        return false;
                    }
                }
                else if (type == 2)
                {
                    // Dynamic Huffman tables.
                    uint32_t literalCount = 0, distanceCount = 0, codeCount = 0;
                    if (!s.ReceiveBits(5, literalCount) || !s.ReceiveBits(5, distanceCount) || !s.ReceiveBits(4, codeCount))
                    {
                        return false;
                    }
                    literalCount += 257;
                    distanceCount += 1;
                    codeCount += 4;
                    if (literalCount > 286 || distanceCount > 30)
                    {
                        return false;
                    }

                    uint8_t codeLengths[19]{};
                    for (uint32_t i = 0; i < codeCount; ++i)
                    {
                        uint32_t bits = 0;
                        if (!s.ReceiveBits(3, bits))
                        {
                            return false;
                        }
                        codeLengths[CodeLengthOrder[i]] = static_cast<uint8_t>(bits);
                    }
                    Huffman codeTable;
                    if (!BuildHuffman(codeTable, codeLengths, 19))
                    {
                        return false;
                    }

                    uint8_t allLengths[286 + 30]{};
                    size_t position = 0;
                    const auto total = literalCount + distanceCount;
                    while (position < total)
                    {
                        const auto symbol = s.Decode(codeTable);
                        if (symbol < 0)
                        {
                            return false;
                        }
                        if (symbol < 16)
                        {
                            allLengths[position++] = static_cast<uint8_t>(symbol);
                        }
                        else
                        {
                            uint8_t value = 0;
                            int repeat = 0;
                            if (symbol == 16)
                            {
                                if (position == 0)
                                {
                                    return false;
                                }
                                value = allLengths[position - 1];
                                uint32_t bits = 0;
                                if (!s.ReceiveBits(2, bits))
                                {
                                    return false;
                                }
                                repeat = 3 + static_cast<int>(bits);
                            }
                            else if (symbol == 17)
                            {
                                uint32_t bits = 0;
                                if (!s.ReceiveBits(3, bits))
                                {
                                    return false;
                                }
                                repeat = 3 + static_cast<int>(bits);
                            }
                            else
                            {
                                uint32_t bits = 0;
                                if (!s.ReceiveBits(7, bits))
                                {
                                    return false;
                                }
                                repeat = 11 + static_cast<int>(bits);
                            }
                            if (position + static_cast<size_t>(repeat) > total)
                            {
                                return false;
                            }
                            while (repeat--)
                            {
                                allLengths[position++] = value;
                            }
                        }
                    }
                    if (allLengths[256] == 0)
                    {
                        return false; // no end-of-block code
                    }
                    if (!BuildHuffman(literals, allLengths, static_cast<int>(literalCount)) ||
                        !BuildHuffman(distances, allLengths + literalCount, static_cast<int>(distanceCount)))
                    {
                        return false;
                    }
                    if (!InflateBlock(s, literals, distances))
                    {
                        return false;
                    }
                }
                else
                {
                    return false; // reserved block type
                }

                if (finalBit)
                {
                    return true;
                }
            }
        }
    }

    std::optional<std::vector<uint8_t>> Inflate(const std::span<const uint8_t> input, const size_t maxOutput)
    {
        if (input.size() < 6)
        {
            return std::nullopt; // header + empty stream + trailer
        }

        // RFC 1950 header: CM must be 8, CINFO at most 7, the check
        // value must be a multiple of 31, and preset dictionaries are
        // not supported.
        const auto cmf = input[0];
        const auto flg = input[1];
        if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf * 256 + flg) % 31) != 0 || (flg & 0x20))
        {
            return std::nullopt;
        }

        Inflater s{ .data = input.data() + 2, .size = input.size() - 2 - 4, .maxOutput = maxOutput };
        if (!InflateRaw(s))
        {
            return std::nullopt;
        }

        // Verify the Adler-32 trailer: big-endian, b in the high half.
        const auto trailer = input.data() + input.size() - 4;
        const auto expected = (static_cast<uint32_t>(trailer[0]) << 24) | (static_cast<uint32_t>(trailer[1]) << 16) |
                              (static_cast<uint32_t>(trailer[2]) << 8) | static_cast<uint32_t>(trailer[3]);
        uint32_t a = 1;
        uint32_t b = 0;
        for (const auto byte : s.out)
        {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        if ((((b << 16) | a)) != expected)
        {
            return std::nullopt;
        }

        return std::move(s.out);
    }
}
