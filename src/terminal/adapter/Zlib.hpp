/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Zlib.hpp

Abstract:
- Decompression of zlib (RFC 1950) streams as used by the compressed
  transmission medium of the Kitty Graphics Protocol (o=z). Implemented
  as a compact DEFLATE (RFC 1951) decoder covering stored, fixed-Huffman
  and dynamic-Huffman blocks, which is everything a conforming zlib
  stream can contain.
--*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Zlib
{
    // Inflates a zlib stream. Fails (nullopt) on malformed input, on a
    // preset dictionary (never sent by the protocol's clients), or when
    // the output grows past maxOutput bytes, which bounds decompression
    // bombs. The Adler-32 trailer is verified.
    std::optional<std::vector<uint8_t>> Inflate(const std::span<const uint8_t> input, const size_t maxOutput);
}
