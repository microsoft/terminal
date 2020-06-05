/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- utf8ToWideCharParser.hpp

Abstract:
- This transforms a multi-byte character sequence into wide chars
- It will attempt to work around invalid byte sequences
- Partial byte sequences are supported

Author(s):
- Austin Diviness (AustDi) 16-August-2016
--*/

#pragma once

class Utf8ToWideCharParser final
{
public:
    Utf8ToWideCharParser(const unsigned int codePage);
    void SetCodePage(const unsigned int codePage);
    [[nodiscard]] HRESULT Parse(_In_reads_(cchBuffer) const byte* const pBytes,
                                _In_ unsigned int const cchBuffer,
                                _Out_ unsigned int& cchConsumed,
                                _Inout_ std::unique_ptr<wchar_t[]>& converted,
                                _Out_ unsigned int& cchConverted);

private:
    enum class _State
    {
        Ready, // ready for input, no partially parsed code points
        Error, // error in parsing given bytes
        BeginPartialParse, // not a clean byte sequence, needs involved parsing
        AwaitingMoreBytes, // have a partial sequence saved, waiting for the rest of it
        Finished // ready to return a wide char sequence
    };

    bool _IsLeadByte(_In_ byte ch);
    bool _IsContinuationByte(_In_ byte ch);
    bool _IsAsciiByte(_In_ byte ch);
    bool _IsValidMultiByteSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb);
    bool _IsPartialMultiByteSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb);
    unsigned int _Utf8SequenceSize(_In_ byte ch);
    unsigned int _ParseFullRange(_In_reads_(cb) const byte* const _InputChars, const unsigned int cb);
    unsigned int _InvolvedParse(_In_reads_(cb) const byte* const pInputChars, const unsigned int cb);
    std::pair<std::unique_ptr<byte[]>, unsigned int> _RemoveInvalidSequences(_In_reads_(cb) const byte* const pInputChars,
                                                                             const unsigned int cb);
    void _StorePartialSequence(_In_reads_(cb) const byte* const pLeadByte, const unsigned int cb);
    void _Reset();

    static const unsigned int _UTF8_BYTE_SEQUENCE_MAX = 4;

    byte _utf8CodePointPieces[_UTF8_BYTE_SEQUENCE_MAX];
    unsigned int _bytesStored; // bytes stored in utf8CodePointPieces
    unsigned int _currentCodePage;
    std::unique_ptr<wchar_t[]> _convertedWideChars;
    _State _currentState;

#ifdef UNIT_TESTING
    friend class Utf8ToWideCharParserTests;
#endif
};
