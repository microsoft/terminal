//--------------------------------------------------------------------------------------
// File: BinaryReader.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "BinaryReader.h"

using namespace DirectX;


// Constructor reads from the filesystem.
BinaryReader::BinaryReader(_In_z_ wchar_t const* fileName) noexcept(false) :
    mPos(nullptr),
    mEnd(nullptr)
{
    size_t dataSize;

    HRESULT hr = ReadEntireFile(fileName, mOwnedData, &dataSize);
    if (FAILED(hr))
    {
        DebugTrace("ERROR: BinaryReader failed (%08X) to load '%ls'\n",
            static_cast<unsigned int>(hr), fileName);
        throw std::runtime_error("BinaryReader");
    }

    mPos = mOwnedData.get();
    mEnd = mOwnedData.get() + dataSize;
}


// Constructor reads from an existing memory buffer.
BinaryReader::BinaryReader(_In_reads_bytes_(dataSize) uint8_t const* dataBlob, size_t dataSize) noexcept :
    mPos(dataBlob),
    mEnd(dataBlob + dataSize)
{
}


// Reads from the filesystem into memory.
HRESULT BinaryReader::ReadEntireFile(
    _In_z_ wchar_t const* fileName,
    _Inout_ std::unique_ptr<uint8_t[]>& data,
    _Out_ size_t* dataSize)
{
    if (!fileName || !dataSize)
        return E_INVALIDARG;

    *dataSize = 0;

    // Open the file.
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(
        fileName,
        GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
        nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(
        fileName,
        GENERIC_READ, FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr)));
#endif

    if (!hFile)
        return HRESULT_FROM_WIN32(GetLastError());

    // Get the file size.
    FILE_STANDARD_INFO fileInfo;
    if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // File is too big for 32-bit allocation, so reject read.
    if (fileInfo.EndOfFile.HighPart > 0)
        return E_FAIL;

    // Create enough space for the file data.
    data.reset(new uint8_t[fileInfo.EndOfFile.LowPart]);

    if (!data)
        return E_OUTOFMEMORY;

    // Read the data in.
    DWORD bytesRead = 0;

    if (!ReadFile(hFile.get(), data.get(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (bytesRead < fileInfo.EndOfFile.LowPart)
        return E_FAIL;

    *dataSize = bytesRead;

    return S_OK;
}
