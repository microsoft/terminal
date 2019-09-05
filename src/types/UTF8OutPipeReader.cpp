// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Utf8OutPipeReader.hpp"
#include <type_traits>
#include <utility>

UTF8OutPipeReader::UTF8OutPipeReader(HANDLE outPipe) noexcept :
    _outPipe{ outPipe },
    _buffer{ 0 },
    _utf8Partials{ 0 }
{
}

// Method Description:
//   Populates a string_view with *complete* UTF-8 codepoints read from the pipe.
//   If it receives an incomplete codepoint, it will cache it until it can be completed.
//   Note: This method trusts that the other end will, in fact, send complete codepoints.
// Arguments:
//   - strView: on return, populated with successfully-read codepoints.
// Return Value:
//   An HRESULT indicating whether the read was successful. For the purposes of this
//   method, a closed pipe is considered a successful (but false!) read. All other errors
//   are translated into an appropriate status code.
//   S_OK for a successful read
//   S_FALSE for a read on a closed pipe
//   E_* (anything) for a failed read
[[nodiscard]] HRESULT UTF8OutPipeReader::Read(_Out_ std::string_view& strView)
{
    DWORD dwRead{};
    bool fSuccess{};

    // in case of early escaping
    _buffer.at(0) = 0;
    strView = std::string_view{ _buffer.data(), 0 };

    // copy UTF-8 code units that were remaining from the previously read chunk (if any)
    if (_dwPartialsLen != 0)
    {
        std::move(_utf8Partials.cbegin(), _utf8Partials.cbegin() + _dwPartialsLen, _buffer.begin());
    }

    // try to read data
    fSuccess = !!ReadFile(_outPipe, &_buffer.at(_dwPartialsLen), gsl::narrow<DWORD>(_buffer.size()) - _dwPartialsLen, &dwRead, nullptr);

    dwRead += _dwPartialsLen;
    _dwPartialsLen = 0;

    if (!fSuccess) // reading failed (we must check this first, because dwRead will also be 0.)
    {
        const auto lastError = GetLastError();
        if (lastError == ERROR_BROKEN_PIPE)
        {
            // This is a successful, but detectable, exit.
            // There is a chance that we put some partials into the buffer. Since
            // the pipe has closed, they're just invalid now. They're not worth
            // reporting.
            return S_FALSE;
        }

        return HRESULT_FROM_WIN32(lastError);
    }

    if (dwRead == 0) // quit if no data has been read and no cached data was left over
    {
        return S_OK;
    }

    const auto endPtr = _buffer.cbegin() + dwRead;
    auto backIter = endPtr - 1;
    // If the last byte in the buffer was a byte belonging to a UTF-8 multi-byte character
    if ((*backIter & _Utf8BitMasks::MaskAsciiByte) > _Utf8BitMasks::IsAsciiByte)
    {
        // Check only up to 3 last bytes, if no Lead Byte was found then the byte before must be the Lead Byte and no partials are in the buffer
        for (DWORD dwSequenceLen{ 1UL }; dwSequenceLen < std::min(dwRead, 4UL); ++dwSequenceLen, --backIter)
        {
            // If Lead Byte found
            if ((*backIter & _Utf8BitMasks::MaskContinuationByte) > _Utf8BitMasks::IsContinuationByte)
            {
                // If the Lead Byte indicates that the last bytes in the buffer is a partial UTF-8 code point then cache them:
                //  Use the bitmask at index `dwSequenceLen`. Compare the result with the operand having the same index. If they
                //  are not equal then the sequence has to be cached because it is a partial code point. Otherwise the
                //  sequence is a complete UTF-8 code point and the whole buffer is ready for the conversion to hstring.
                if ((*backIter & _cmpMasks.at(dwSequenceLen)) != _cmpOperands.at(dwSequenceLen))
                {
                    std::move(backIter, endPtr, _utf8Partials.begin());
                    dwRead -= dwSequenceLen;
                    _dwPartialsLen = dwSequenceLen;
                }

                break;
            }
        }
    }

    // give back a view of the part of the buffer that contains complete code points only
    strView = std::string_view{ &_buffer.at(0), dwRead };
    return S_OK;
}
