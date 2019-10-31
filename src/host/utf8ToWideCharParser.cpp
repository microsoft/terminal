// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "utf8ToWideCharParser.hpp"
#include <unicode.hpp>

#ifndef WIL_ENABLE_EXCEPTIONS
#error WIL exception helpers must be enabled
#endif

#define IsBitSet WI_IsFlagSet

const byte NonAsciiBytePrefix = 0x80;

const byte ContinuationByteMask = 0xC0;
const byte ContinuationBytePrefix = 0x80;

const byte MostSignificantBitMask = 0x80;

// Routine Description:
// - Constructs an instance of the parser.
// Arguments:
// - codePage - Starting code page to interpret input with.
// Return Value:
// - A new instance of the parser.
Utf8ToWideCharParser::Utf8ToWideCharParser(const unsigned int codePage) :
    _currentCodePage{ codePage },
    _bytesStored{ 0 },
    _currentState{ _State::Ready },
    _convertedWideChars{ nullptr }
{
    std::fill_n(_utf8CodePointPieces, _UTF8_BYTE_SEQUENCE_MAX, 0ui8);
}

// Routine Description:
// - Set the code page that input sequences will correspond to. Clears
// any saved partial multi-byte sequences if the code page changes
// from the code page the partial sequence is associated with.
// Arguments:
// - codePage - the code page to set to.
// Return Value:
// - <none>
void Utf8ToWideCharParser::SetCodePage(const unsigned int codePage)
{
    if (_currentCodePage != codePage)
    {
        _currentCodePage = codePage;
        // we can't be making any assumptions about the partial
        // sequence we were storing now that the codepage has changed
        _bytesStored = 0;
        _currentState = _State::Ready;
    }
}

// Routine Description:
// - Parses the input multi-byte sequence.
// Arguments:
// - pBytes - The byte sequence to parse.
// - cchBuffer - The amount of bytes in pBytes. This will contain the
// number of wide chars contained by converted after this function is
// run, or 0 if an error occurs (or if pBytes is 0).
// - converted - a valid unique_ptr to store the parsed wide chars
// in. On error this will contain nullptr instead of an array.
// Return Value:
// - <none>
[[nodiscard]] HRESULT Utf8ToWideCharParser::Parse(_In_reads_(cchBuffer) const byte* const pBytes,
                                                  _In_ unsigned int const cchBuffer,
                                                  _Out_ unsigned int& cchConsumed,
                                                  _Inout_ std::unique_ptr<wchar_t[]>& converted,
                                                  _Out_ unsigned int& cchConverted)
{
    cchConsumed = 0;
    cchConverted = 0;

    // we can't parse anything if we weren't given any data to parse
    if (cchBuffer == 0)
    {
        return S_OK;
    }
    // we shouldn't be parsing if the current codepage isn't UTF8
    if (_currentCodePage != CP_UTF8)
    {
        _currentState = _State::Error;
    }
    HRESULT hr = S_OK;
    try
    {
        bool loop = true;
        unsigned int wideCharCount = 0;
        _convertedWideChars.reset(nullptr);
        while (loop)
        {
            switch (_currentState)
            {
            case _State::Ready:
                wideCharCount = _ParseFullRange(pBytes, cchBuffer);
                break;
            case _State::BeginPartialParse:
                wideCharCount = _InvolvedParse(pBytes, cchBuffer);
                break;
            case _State::Error:
                hr = E_FAIL;
                _Reset();
                wideCharCount = 0;
                loop = false;
                break;
            case _State::Finished:
                _currentState = _State::Ready;
                cchConsumed = cchBuffer;
                loop = false;
                break;
            case _State::AwaitingMoreBytes:
                _currentState = _State::BeginPartialParse;
                cchConsumed = cchBuffer;
                loop = false;
                break;
            default:
                _currentState = _State::Error;
                break;
            }
        }
        converted.swap(_convertedWideChars);
        cchConverted = wideCharCount;
    }
    catch (...)
    {
        _Reset();
        hr = wil::ResultFromCaughtException();
    }
    return hr;
}

// Routine Description:
// - Determines if ch is a UTF8 lead byte. See _Utf8SequenceSize() for a
// description of how a lead byte is specified.
// Arguments:
// - ch - The byte to test.
// Return Value:
// - True if ch is a lead byte, false otherwise.
bool Utf8ToWideCharParser::_IsLeadByte(_In_ byte ch)
{
    unsigned int sequenceSize = _Utf8SequenceSize(ch);
    return !_IsContinuationByte(ch) &&
           !_IsAsciiByte(ch) &&
           sequenceSize > 1 &&
           sequenceSize <= _UTF8_BYTE_SEQUENCE_MAX;
}

// Routine Description:
// - Determines if ch is a UTF8 continuation byte. A continuation byte
// takes the form 10xx xxxx, so we need to check that the two most
// significant bits are a 1 followed by a 0.
// Arguments:
// - ch - The byte to test
// Return Value:
// - True if ch is a continuation byte, false otherwise.
bool Utf8ToWideCharParser::_IsContinuationByte(_In_ byte ch)
{
    return (ch & ContinuationByteMask) == ContinuationBytePrefix;
}

// Routine Description:
// - Determines if ch is an ASCII compatible UTF8 byte. A byte is
// ASCII compatible if the most significant bit is a 0.
// Arguments:
// - ch - The byte to test.
// Return Value:
// - True if ch is an ASCII compatible byte, false otherwise.
bool Utf8ToWideCharParser::_IsAsciiByte(_In_ byte ch)
{
    return !IsBitSet(ch, NonAsciiBytePrefix);
}

// Routine Description:
// - Determines if the sequence starting at pLeadByte is a valid UTF8
// multi-byte sequence. Note that a single ASCII byte does not count
// as a valid MULTI-byte sequence.
// Arguments:
// - pLeadByte - The start of a possible sequence.
// - cb - The amount of remaining chars in the array that
// pLeadByte points to.
// Return Value:
// - true if the sequence starting at pLeadByte is a multi-byte
// sequence and uses all of the remaining chars, false otherwise.
bool Utf8ToWideCharParser::_IsValidMultiByteSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb)
{
    if (!_IsLeadByte(*pLeadByte))
    {
        return false;
    }
    const unsigned int sequenceSize = _Utf8SequenceSize(*pLeadByte);
    if (sequenceSize > cb)
    {
        return false;
    }
    // i starts at 1 so that we skip the lead byte
    for (unsigned int i = 1; i < sequenceSize; ++i)
    {
        const byte ch = *(pLeadByte + i);
        if (!_IsContinuationByte(ch))
        {
            return false;
        }
    }
    return true;
}

// Routine Description:
// - Checks if the sequence starting at pLeadByte is a portion of a
// single valid multi-byte sequence. A new sequence must not be
// started within the range provided in order for it to be considered
// a valid partial sequence.
// Arguments:
// - pLeadByte - The start of the possible partial sequence.
// - cb - The amount of remaining chars in the array that
// pLeadByte points to.
// Return Value:
// - true if the sequence is a single partial multi-byte sequence,
// false otherwise.
bool Utf8ToWideCharParser::_IsPartialMultiByteSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb)
{
    if (!_IsLeadByte(*pLeadByte))
    {
        return false;
    }
    const unsigned int sequenceSize = _Utf8SequenceSize(*pLeadByte);
    if (sequenceSize <= cb)
    {
        return false;
    }
    // i starts at 1 so that we skip the lead byte
    for (unsigned int i = 1; i < cb; ++i)
    {
        const byte ch = *(pLeadByte + i);
        if (!_IsContinuationByte(ch))
        {
            return false;
        }
    }
    return true;
}

// Routine Description:
// - Determines the number of bytes in the UTF8 multi-byte sequence.
// Does not perform any verification that ch is a valid lead byte. A
// lead byte indicates how many bytes are in a sequence by repeating a
// 1 for each byte in the sequence, starting with the most significant
// bit, then a 0 directly after. Ex:
// - 110x xxxx = a two byte sequence
// - 1110 xxxx = a three byte sequence
//
// Note that a byte that has a pattern 10xx xxxx is a continuation
// byte and will be reported as a sequence of one by this function.
//
// A sequence is currently a maximum of four bytes but this function
// will just count the number of consecutive 1 bits (starting with the
// most significant bit) so if the byte is malformed (ex. 1111 110x) a
// number larger than the maximum utf8 byte sequence may be
// returned. It is the responsibility of the calling function to check
// this (and the continuation byte scenario) because we don't do any
// verification here.
// Arguments:
// - ch - the lead byte of a UTF8 multi-byte sequence.
// Return Value:
// - The number of bytes (including the lead byte) that ch indicates
// are in the sequence.
unsigned int Utf8ToWideCharParser::_Utf8SequenceSize(_In_ byte ch)
{
    unsigned int msbOnes = 0;
    while (IsBitSet(ch, MostSignificantBitMask))
    {
        ++msbOnes;
        ch <<= 1;
    }
    return msbOnes;
}

// Routine Description:
// - Attempts to parse pInputChars by themselves in wide chars,
// without using any saved partial byte sequences. On success,
// _convertedWideChars will contain the converted wide char sequence
// and _currentState will be set to _State::Finished. On failure,
// _currentState will be set to either _State::Error or
// _State::BeginPartialParse.
// Arguments:
// - pInputChars - The byte sequence to convert to wide chars.
// - cb - The amount of bytes in pInputChars.
// Return Value:
// - The amount of wide chars that are stored in _convertedWideChars,
// or 0 if pInputChars cannot be successfully converted.
unsigned int Utf8ToWideCharParser::_ParseFullRange(_In_reads_(cb) const byte* const pInputChars, const unsigned int cb)
{
    int bufferSize = MultiByteToWideChar(_currentCodePage,
                                         MB_ERR_INVALID_CHARS,
                                         reinterpret_cast<LPCCH>(pInputChars),
                                         cb,
                                         nullptr,
                                         0);
    if (bufferSize == 0)
    {
        DWORD err = GetLastError();
        LOG_WIN32(err);
        if (err == ERROR_NO_UNICODE_TRANSLATION)
        {
            _currentState = _State::BeginPartialParse;
        }
        else
        {
            _currentState = _State::Error;
        }
    }
    else
    {
        _convertedWideChars = std::make_unique<wchar_t[]>(bufferSize);
        bufferSize = MultiByteToWideChar(_currentCodePage,
                                         0,
                                         reinterpret_cast<LPCCH>(pInputChars),
                                         cb,
                                         _convertedWideChars.get(),
                                         bufferSize);
        if (bufferSize == 0)
        {
            LOG_LAST_ERROR();
            _currentState = _State::Error;
        }
        else
        {
            _currentState = _State::Finished;
        }
    }
    return bufferSize;
}

// Routine Description:
// - Attempts to parse pInputChars in a more complex manner, taking
// into account any previously saved partial byte sequences while
// removing any invalid byte sequences. Will also save a partial byte
// sequence from the end of the sequence if necessary. If the sequence
// can be successfully parsed, _currentState will be set to
// _State::Finished. If more bytes are necessary to form a wide char,
// then _currentState will be set to
// _State::AwaitingMoreBytes. Otherwise, _currentState will be set to
// _State::Error.
// Arguments:
// - pInputChars - The byte sequence to convert to wide chars.
// - cb - The amount of bytes in pInputChars.
// Return Value:
// - The amount of wide chars that are stored in _convertedWideChars,
// or 0 if pInputChars cannot be successfully converted or if the
// parser requires additional bytes before returning a valid wide
// char.
unsigned int Utf8ToWideCharParser::_InvolvedParse(_In_reads_(cb) const byte* const pInputChars, const unsigned int cb)
{
    // Do safe math to add up the count and error if it won't fit.
    unsigned int count;
    const HRESULT hr = UIntAdd(cb, _bytesStored, &count);
    if (FAILED(hr))
    {
        LOG_HR(hr);
        _currentState = _State::Error;
        return 0;
    }

    // Allocate space and copy.
    std::unique_ptr<byte[]> combinedInputBytes = std::make_unique<byte[]>(count);
    std::copy(_utf8CodePointPieces, _utf8CodePointPieces + _bytesStored, combinedInputBytes.get());
    std::copy(pInputChars, pInputChars + cb, combinedInputBytes.get() + _bytesStored);
    _bytesStored = 0;
    std::pair<std::unique_ptr<byte[]>, unsigned int> validSequence = _RemoveInvalidSequences(combinedInputBytes.get(), count);
    // the input may have only been a partial sequence so we need to
    // check that there are actually any bytes that we can convert
    // right now
    if (validSequence.second == 0 && _bytesStored > 0)
    {
        _currentState = _State::AwaitingMoreBytes;
        return 0;
    }

    // By this point, all obviously invalid sequences have been removed.
    // But non-minimal forms of sequences might still exist.
    // MB2WC will fail non-minimal forms with MB_ERR_INVALID_CHARS at this point.
    // So we call with flags = 0 such that non-minimal forms get the U+FFFD
    // replacement character treatment.
    // This issue and related concerns are fully captured in future work item GH#3378
    // for future cleanup and reconciliation.
    // The original issue introducing this was GH#3320.
    int bufferSize = MultiByteToWideChar(_currentCodePage,
                                         0,
                                         reinterpret_cast<LPCCH>(validSequence.first.get()),
                                         validSequence.second,
                                         nullptr,
                                         0);
    if (bufferSize == 0)
    {
        LOG_LAST_ERROR();
        _currentState = _State::Error;
    }
    else
    {
        _convertedWideChars = std::make_unique<wchar_t[]>(bufferSize);
        bufferSize = MultiByteToWideChar(_currentCodePage,
                                         0,
                                         reinterpret_cast<LPCCH>(validSequence.first.get()),
                                         validSequence.second,
                                         _convertedWideChars.get(),
                                         bufferSize);
        if (bufferSize == 0)
        {
            LOG_LAST_ERROR();
            _currentState = _State::Error;
        }
        else if (_bytesStored > 0)
        {
            _currentState = _State::AwaitingMoreBytes;
        }
        else
        {
            _currentState = _State::Finished;
        }
    }
    return bufferSize;
}

// Routine Description:
// - Reads pInputChars byte by byte, removing any invalid UTF8
// multi-byte sequences.
// Arguments:
// - pInputChars - The byte sequence to fix.
// - cb - The amount of bytes in pInputChars.
// Return Value:
// - A std::pair containing the corrected byte sequence and the number
// of bytes in the sequence.
std::pair<std::unique_ptr<byte[]>, unsigned int> Utf8ToWideCharParser::_RemoveInvalidSequences(_In_reads_(cb) const byte* const pInputChars, const unsigned int cb)
{
    std::unique_ptr<byte[]> validSequence = std::make_unique<byte[]>(cb);
    unsigned int validSequenceLocation = 0; // index into validSequence
    unsigned int currentByteInput = 0; // index into pInputChars
    while (currentByteInput < cb)
    {
        if (_IsAsciiByte(pInputChars[currentByteInput]))
        {
            validSequence[validSequenceLocation] = pInputChars[currentByteInput];
            ++validSequenceLocation;
            ++currentByteInput;
        }
        else if (_IsContinuationByte(pInputChars[currentByteInput]))
        {
            while (currentByteInput < cb && _IsContinuationByte(pInputChars[currentByteInput]))
            {
                ++currentByteInput;
            }
        }
        else if (_IsLeadByte(pInputChars[currentByteInput]))
        {
            if (_IsValidMultiByteSequence(&pInputChars[currentByteInput], cb - currentByteInput))
            {
                const unsigned int sequenceSize = _Utf8SequenceSize(pInputChars[currentByteInput]);
                // min is to guard against static analyis possible buffer overflow
                const unsigned int limit = std::min(sequenceSize, cb - currentByteInput);
                for (unsigned int i = 0; i < limit; ++i)
                {
                    validSequence[validSequenceLocation] = pInputChars[currentByteInput];
                    ++validSequenceLocation;
                    ++currentByteInput;
                }
            }
            else if (_IsPartialMultiByteSequence(&pInputChars[currentByteInput], cb - currentByteInput))
            {
                _StorePartialSequence(&pInputChars[currentByteInput], cb - currentByteInput);
                break;
            }
            else
            {
                ++currentByteInput;
                while (currentByteInput < cb && _IsContinuationByte(pInputChars[currentByteInput]))
                {
                    ++currentByteInput;
                }
            }
        }
        else
        {
            // invalid byte, skip it.
            ++currentByteInput;
        }
    }
    return std::make_pair<std::unique_ptr<byte[]>, unsigned int>(std::move(validSequence), std::move(validSequenceLocation));
}

// Routine Description:
// - Stores a partial byte sequence for later use. Will overwrite any
// previously saved sequence. Will only store bytes up to the limit
// Utf8ToWideCharParser::_UTF8_BYTE_SEQUENCE_MAX.
// Arguments:
// - pLeadByte - The beginning of the sequence to save.
// - cb - The amount of bytes to save.
// Return Value:
// - <none>
void Utf8ToWideCharParser::_StorePartialSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb)
{
    const unsigned int maxLength = std::min(cb, _UTF8_BYTE_SEQUENCE_MAX);
    std::copy(pLeadByte, pLeadByte + maxLength, _utf8CodePointPieces);
    _bytesStored = maxLength;
}

// Routine Description:
// - Resets the state of the parser to that of a newly initialized
// instance. _currentCodePage is not affected.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Utf8ToWideCharParser::_Reset()
{
    _currentState = _State::Ready;
    _bytesStored = 0;
    _convertedWideChars.reset(nullptr);
}
