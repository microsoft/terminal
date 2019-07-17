// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Utf8OutPipeReader.hpp"
#include <type_traits>
#include <utility>

UTF8OutPipeReader::UTF8OutPipeReader(wil::unique_hfile& outPipe) :
    _outPipe{ outPipe }
{
}

[[nodiscard]] HRESULT UTF8OutPipeReader::Read(_Out_ std::string_view& strView)
{
    DWORD dwRead{};
    bool fSuccess{};

    // in case of early escaping
    *_buffer = 0;
    strView = reinterpret_cast<char*>(_buffer);

    // copy UTF-8 code units that were remaining from the previously read chunk (if any)
    if (_dwPartialsLen != 0)
    {
        std::move(_utf8Partials, _utf8Partials + _dwPartialsLen, _buffer);
    }

    // try to read data
    fSuccess = !!ReadFile(_outPipe.get(), &_buffer[_dwPartialsLen], std::extent<decltype(_buffer)>::value - _dwPartialsLen, &dwRead, nullptr);

    dwRead += _dwPartialsLen;
    _dwPartialsLen = 0;

    if (!fSuccess) // reading failed (we must check this first, because dwRead will also be 0.)
    {
        return E_FAIL;
    }
    else if (dwRead == 0) // quit if no data has been read and no cached data was left over
    {
        return S_OK;
    }

    const BYTE* const endPtr{ _buffer + dwRead };
    const BYTE* backIter{ endPtr - 1 };
    // If the last byte in the buffer was a byte belonging to a UTF-8 multi-byte character
    if ((*backIter & _Utf8BitMasks::MaskAsciiByte) > _Utf8BitMasks::IsAsciiByte)
    {
        // Check only up to 3 last bytes, if no Lead Byte was found then the byte before must be the Lead Byte and no partials are in the buffer
        for (DWORD dwSequenceLen{ 1UL }, stop{ dwRead < 4UL ? dwRead : 4UL }; dwSequenceLen < stop; ++dwSequenceLen, --backIter)
        {
            // If Lead Byte found
            if ((*backIter & _Utf8BitMasks::MaskContinuationByte) > _Utf8BitMasks::IsContinuationByte)
            {
                // If the Lead Byte indicates that the last bytes in the buffer is a partial UTF-8 code point then cache them:
                //  Use the bitmask at index `dwSequenceLen`. Compare the result with the operand having the same index. If they
                //  are not equal then the sequence has to be cached because it is a partial code point. Otherwise the
                //  sequence is a complete UTF-8 code point and the whole buffer is ready for the conversion to hstring.
                if ((*backIter & _cmpMasks[dwSequenceLen]) != _cmpOperands[dwSequenceLen])
                {
                    std::move(backIter, endPtr, _utf8Partials);
                    dwRead -= dwSequenceLen;
                    _dwPartialsLen = dwSequenceLen;
                }

                break;
            }
        }
    }

    // give back a view of the part of the buffer that contains complete code points only
    strView = std::string_view{ reinterpret_cast<char*>(_buffer), dwRead };
    return S_OK;
}
