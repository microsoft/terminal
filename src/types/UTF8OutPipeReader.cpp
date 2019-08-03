// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Utf8OutPipeReader.hpp"

UTF8OutPipeReader::UTF8OutPipeReader(HANDLE outPipe) :
    _outPipe{ outPipe }
{
}

// Method Description:
//   Populates a string_view with *complete* UTF-8 codepoints read from the pipe.
//   If it receives an incomplete codepoint, it will cache it until it can be completed.
//   Furthermore it populates only complete composite characters.
//   Note: This method trusts that the other end will, in fact, send complete codepoints.
// Arguments:
//   - utf8StrView: on return, populated with successfully-read codepoints.
// Return Value:
//   An HRESULT indicating whether the read was successful. For the purposes of this
//   method, a closed pipe is considered a successful (but false!) read. All other errors
//   are translated into an appropriate status code.
//   S_OK for a successful read
//   S_FALSE for a read on a closed pipe
//   E_* (anything) for a failed read
[[nodiscard]] HRESULT UTF8OutPipeReader::Read(_Out_ std::string_view& utf8StrView)
{
    DWORD dwRead{};
    bool fSuccess{};
    bool fBufferFull{};

    // in case of early escaping
    utf8StrView = std::string_view{ reinterpret_cast<char*>(_buffer), 0 };

    // copy UTF-8 code units that were remaining from the previously read chunk (if any)
    if (_dwNonCombiningLen != 0)
    {
        std::move(_utf8NonCombining, _utf8NonCombining + _dwNonCombiningLen, _buffer);
    }

    if (_dwPartialsLen != 0)
    {
        std::move(_utf8Partials, _utf8Partials + _dwPartialsLen, _buffer + _dwNonCombiningLen);
    }

    // try to read data
    fSuccess = !!ReadFile(_outPipe, &_buffer[_dwNonCombiningLen + _dwPartialsLen], std::extent<decltype(_buffer)>::value - _dwNonCombiningLen - _dwPartialsLen, &dwRead, nullptr);

    if (!fSuccess) // reading failed (we must check this first, because dwRead will also be 0.)
    {
        auto lastError = GetLastError();

        // This is a serous reading error.
        if (lastError != ERROR_BROKEN_PIPE)
        {
            return HRESULT_FROM_WIN32(lastError);
        }

        // This is a successful, but detectable, exit.
        // There is a chance that we put some partials into the buffer. Since
        // the pipe has closed, they're just invalid now. They're not worth
        // reporting.
        if (_dwNonCombiningLen == 0)
        {
            _dwPartialsLen = 0;
            return S_FALSE;
        }

        // At that point we ignore partials and leave only the cached last character.
        dwRead += _dwNonCombiningLen;
    }
    else
    {
        dwRead += _dwNonCombiningLen + _dwPartialsLen;
    }

    _dwPartialsLen = 0;
    _dwNonCombiningLen = 0;

    if (dwRead == 0) // quit if no data has been read and no cached data was left over
    {
        return S_OK;
    }

    fBufferFull = (std::extent<decltype(_buffer)>::value == dwRead);

    // Cache UTF-8 partials from the end of the chunk read, if any
    const BYTE* endPtr{ _buffer + dwRead };
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

    // Composite characters are expected only from external sources like files. Thus, split composite
    //  characters may only appear if a big amount of data fills the buffer at once. This is only
    //  situation where we cache the last character, too.
    // This caching must not be applied to keyboard input because it would lead to an delay of one
    //  key stroke between input and output.
    if (fBufferFull)
    {
        endPtr = _buffer + dwRead;
        backIter = endPtr - 1;
        // Check up to 4 last bytes for the begin of the last complete character
        for (DWORD dwSequenceLen{ 1UL }, stop{ dwRead < 5UL ? dwRead : 5UL }; dwSequenceLen < stop; ++dwSequenceLen, --backIter)
        {
            // If either an ASCII byte or a Lead Byte is found
            if ((*backIter & _Utf8BitMasks::MaskAsciiByte) == _Utf8BitMasks::IsAsciiByte || (*backIter & _Utf8BitMasks::MaskContinuationByte) > _Utf8BitMasks::IsContinuationByte)
            {
                // If the character is either of the Combining Diacritical Marks then don't cache it
                if (dwSequenceLen == 2)
                {
                    if (std::memcmp(backIter, _combiningMarks.diacriticalBasic.first, dwSequenceLen) >= 0 && std::memcmp(backIter, _combiningMarks.diacriticalBasic.last, dwSequenceLen) <= 0)
                    {
                        break;
                    }
                }
                else if (dwSequenceLen == 3)
                {
                    if ((std::memcmp(backIter, _combiningMarks.diacriticalExtended.first, dwSequenceLen) >= 0 && std::memcmp(_combiningMarks.diacriticalExtended.last, backIter, dwSequenceLen) <= 0) ||
                        (std::memcmp(backIter, _combiningMarks.diacriticalSupplement.first, dwSequenceLen) >= 0 && std::memcmp(backIter, _combiningMarks.diacriticalSupplement.last, dwSequenceLen) <= 0) ||
                        (std::memcmp(backIter, _combiningMarks.diacriticalForSymbols.first, dwSequenceLen) >= 0 && std::memcmp(backIter, _combiningMarks.diacriticalForSymbols.last, dwSequenceLen) <= 0) ||
                        (std::memcmp(backIter, _combiningMarks.halfMarks.first, dwSequenceLen) >= 0 && std::memcmp(backIter, _combiningMarks.halfMarks.last, dwSequenceLen) <= 0))
                    {
                        break;
                    }
                }

                // otherwise cache it
                std::move(backIter, endPtr, _utf8NonCombining);
                dwRead -= dwSequenceLen;
                _dwNonCombiningLen = dwSequenceLen;
                break;
            }
        }
    }

    // populate a view of the part of the buffer that contains only complete code points and complete composite characters
    utf8StrView = std::string_view{ reinterpret_cast<char*>(_buffer), dwRead };
    return S_OK;
}

// Method Description:
//   Populates a wstring_view with *complete* UTF-16 codepoints read and converted from the pipe.
//   Calls the Read() overload for UTF-8, and _UTF8ToUtf16Precomposed() for the converson to UTF-16.
//   Note: This method trusts that the other end will, in fact, send complete codepoints.
// Arguments:
//   - utf16StrView: on return, populated with successfully-read and converted codepoints.
// Return Value:
//   An HRESULT indicating whether both read and conversion were successful. For the purposes of this
//   method, a closed pipe is considered a successful (but false!) read. All other errors
//   are translated into an appropriate status code.
//   S_OK for a successful read and conversion
//   S_FALSE for a read on a closed pipe
//   E_* (anything) for a failed read or conversion
[[nodiscard]] HRESULT UTF8OutPipeReader::Read(_Out_ std::wstring_view& utf16StrView)
{
    std::string_view utf8StrView{};
    utf16StrView = std::wstring_view{ _precomposedBuffer.get(), 0 };
    HRESULT hr{ Read(utf8StrView) };
    RETURN_HR_IF(hr, hr != S_OK);
    return _UTF8ToUtf16Precomposed(utf8StrView, utf16StrView);
}

// Method Description:
// - Takes a UTF-8 string,
//   allocates memory for the conversions,
//   performs the conversion to UTF-16,
//   converts base + combining characters to the canonical precomposed equivalents,
//   and populates the UTF-16 result as wide string.
//   Note: This routine trusts that the caller passed complete codepoints and
//         complete composite characters (if any).
// Arguments:
// - utf8StrView - Taken view of characters of UTF-8 source text.
// - utf16StrView - Populated view of characters of UTF-16 target text.
// Return Value:
//   S_OK for a successful conversion
//   E_* (anything) for a bad allocation or a failed conversion
[[nodiscard]] HRESULT UTF8OutPipeReader::_UTF8ToUtf16Precomposed(_In_ const std::string_view utf8StrView, _Out_ std::wstring_view& utf16StrView)
{
    if (utf8StrView.empty())
    {
        utf16StrView = std::wstring_view{ _precomposedBuffer.get(), 0 };
        return S_OK;
    }

    int utf8Size{};
    size_t utf16Size{};

    // The length must not exceed the maximum value of an int because both MultiByteToWideChar and FoldStringW use int to represent the size.
    RETURN_IF_FAILED(SizeTToInt(utf8StrView.length(), &utf8Size));

    // Convert UTF-8 to UTF-16.
    int convertedSize = MultiByteToWideChar(static_cast<UINT>(CP_UTF8), 0UL, utf8StrView.data(), utf8Size, nullptr, 0);
    RETURN_LAST_ERROR_IF(0 == convertedSize);
    RETURN_IF_FAILED(IntToSizeT(convertedSize, &utf16Size));
    if (_convertedCapacity < utf16Size)
    {
        _convertedBuffer.reset(new (std::nothrow) wchar_t[utf16Size]);
        if (nullptr == _convertedBuffer.get())
        {
            _convertedCapacity = 0;
            return E_OUTOFMEMORY;
        }

        _convertedCapacity = utf16Size;
    }

    RETURN_LAST_ERROR_IF(0 == MultiByteToWideChar(static_cast<UINT>(CP_UTF8), 0UL, utf8StrView.data(), utf8Size, _convertedBuffer.get(), convertedSize));

    // Convert each base + combining characters to the canonical precomposed equivalent.
    // TODO: Should we pass MAP_FOLDCZONE rather than MAP_PRECOMPOSED to address compatibility characters, too?
    int precomposedSize{ FoldStringW(static_cast<DWORD>(MAP_PRECOMPOSED), _convertedBuffer.get(), convertedSize, nullptr, 0) };
    RETURN_LAST_ERROR_IF(0 == precomposedSize);
    RETURN_IF_FAILED(IntToSizeT(precomposedSize, &utf16Size));
    if (_precomposedCapacity < utf16Size)
    {
        _precomposedBuffer.reset(new (std::nothrow) wchar_t[utf16Size]);
        if (nullptr == _precomposedBuffer.get())
        {
            _precomposedCapacity = 0;
            return E_OUTOFMEMORY;
        }

        _precomposedCapacity = utf16Size;
    }

    RETURN_LAST_ERROR_IF(0 == FoldStringW(static_cast<DWORD>(MAP_PRECOMPOSED), _convertedBuffer.get(), convertedSize, _precomposedBuffer.get(), precomposedSize));

    utf16StrView = std::wstring_view{ _precomposedBuffer.get(), utf16Size };
    return S_OK;
}
