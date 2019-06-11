// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "alias.h"

#include "_output.h"
#include "output.h"
#include "stream.h"
#include "_stream.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "../types/inc/convert.hpp"
#include "srvinit.h"
#include "resource.h"

#include "ApiRoutines.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

struct case_insensitive_hash
{
    std::size_t operator()(const std::wstring& key) const
    {
        std::wstring lower(key);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        std::hash<std::wstring> hash;
        return hash(lower);
    }
};

struct case_insensitive_equality
{
    bool operator()(const std::wstring& lhs, const std::wstring& rhs) const
    {
        return 0 == _wcsicmp(lhs.data(), rhs.data());
    }
};

std::unordered_map<std::wstring,
                   std::unordered_map<std::wstring,
                                      std::wstring,
                                      case_insensitive_hash,
                                      case_insensitive_equality>,
                   case_insensitive_hash,
                   case_insensitive_equality>
    g_aliasData;

// Routine Description:
// - Adds a command line alias to the global set.
// - Converts and calls the W version of this function.
// Arguments:
// - source - The shorthand/alias or source buffer to set
// - target- The destination/expansion or target buffer to set
// - exeName - The client EXE application attached to the host to whom this substitution will apply
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::AddConsoleAliasAImpl(const std::string_view source,
                                                        const std::string_view target,
                                                        const std::string_view exeName) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    UINT const codepage = gci.CP;

    try
    {
        const auto sourceW = ConvertToW(codepage, source);
        const auto targetW = ConvertToW(codepage, target);
        const auto exeNameW = ConvertToW(codepage, exeName);

        return AddConsoleAliasWImpl(sourceW, targetW, exeNameW);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Adds a command line alias to the global set.
// Arguments:
// - source - The shorthand/alias or source buffer to set
// - target - The destination/expansion or target buffer to set
// - exeName - The client EXE application attached to the host to whom this substitution will apply
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::AddConsoleAliasWImpl(const std::wstring_view source,
                                                        const std::wstring_view target,
                                                        const std::wstring_view exeName) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    RETURN_HR_IF(E_INVALIDARG, source.size() == 0);

    try
    {
        std::wstring exeNameString(exeName);
        std::wstring sourceString(source);
        std::wstring targetString(target);

        std::transform(exeNameString.begin(), exeNameString.end(), exeNameString.begin(), towlower);
        std::transform(sourceString.begin(), sourceString.end(), sourceString.begin(), towlower);

        if (targetString.size() == 0)
        {
            // Only try to dig in and erase if the exeName exists.
            auto exeData = g_aliasData.find(exeNameString);
            if (exeData != g_aliasData.end())
            {
                g_aliasData[exeNameString].erase(sourceString);
            }
        }
        else
        {
            // Map will auto-create each level as necessary
            g_aliasData[exeNameString][sourceString] = targetString;
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - source - The shorthand/alias or source buffer to use in lookup
// - target - The destination/expansion or target buffer we are attempting to retrieve. Optionally nullopt to retrieve needed space.
// - writtenOrNeeded - Will specify how many characters were written (if target has value)
//                     or how many characters would have been consumed (if target is nullopt).
// - exeName - The client EXE application attached to the host whose set we should check
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT GetConsoleAliasWImplHelper(const std::wstring_view source,
                                                 std::optional<gsl::span<wchar_t>> target,
                                                 size_t& writtenOrNeeded,
                                                 const std::wstring_view exeName)
{
    // Ensure output variables are initialized
    writtenOrNeeded = 0;

    if (target.has_value() && target.value().size() > 0)
    {
        target.value().at(0) = UNICODE_NULL;
    }

    std::wstring exeNameString(exeName);
    std::wstring sourceString(source);

    // For compatibility, return ERROR_GEN_FAILURE for any result where the alias can't be found.
    // We use .find for the iterators then dereference to search without creating entries.
    const auto exeIter = g_aliasData.find(exeNameString);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), exeIter == g_aliasData.end());
    const auto exeData = exeIter->second;
    const auto sourceIter = exeData.find(sourceString);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), sourceIter == exeData.end());
    const auto targetString = sourceIter->second;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), targetString.size() == 0);

    // TargetLength is a byte count, convert to characters.
    size_t targetSize = targetString.size();
    size_t const cchNull = 1;

    // The total space we need is the length of the string + the null terminator.
    size_t neededSize;
    RETURN_IF_FAILED(SizeTAdd(targetSize, cchNull, &neededSize));

    writtenOrNeeded = neededSize;

    if (target.has_value())
    {
        // if the user didn't give us enough space, return with insufficient buffer code early.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), gsl::narrow<size_t>(target.value().size()) < neededSize);

        RETURN_IF_FAILED(StringCchCopyNW(target.value().data(), target.value().size(), targetString.data(), targetSize));
    }

    return S_OK;
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// - This function will convert input parameters from A to W, call the W version of the routine,
//   and attempt to convert the resulting data back to A for return.
// Arguments:
// - source - The shorthand/alias or source buffer to use in lookup
// - target - The destination/expansion or target buffer we are attempting to retrieve.
// - written - Will specify how many characters were written
// - exeName - The client EXE application attached to the host whose set we should check
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasAImpl(const std::string_view source,
                                                        gsl::span<char> target,
                                                        size_t& written,
                                                        const std::string_view exeName) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    UINT const codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;
    try
    {
        if (target.size() > 0)
        {
            target.at(0) = ANSI_NULL;
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Convert our input parameters to Unicode.
        const auto sourceW = ConvertToW(codepage, source);
        const auto exeNameW = ConvertToW(codepage, exeName);

        // Figure out how big our temporary Unicode buffer must be to retrieve output
        size_t targetNeeded;
        RETURN_IF_FAILED(GetConsoleAliasWImplHelper(sourceW, std::nullopt, targetNeeded, exeNameW));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == targetNeeded);

        // If the user hasn't given us a buffer at all and we need one, return an error.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), 0 == target.size());

        // Allocate a unicode buffer of the right size.
        std::unique_ptr<wchar_t[]> targetBuffer = std::make_unique<wchar_t[]>(targetNeeded);
        RETURN_IF_NULL_ALLOC(targetBuffer);

        // Call the Unicode version of this method
        size_t targetWritten;
        RETURN_IF_FAILED(GetConsoleAliasWImplHelper(sourceW,
                                                    gsl::span<wchar_t>(targetBuffer.get(), targetNeeded),
                                                    targetWritten,
                                                    exeNameW));

        // Set the return size copied to the size given before we attempt to copy.
        // Then multiply by sizeof(wchar_t) due to a long standing bug that we must preserve for compatibility.
        // On failure, the API has historically given back this value.
        written = target.size() * sizeof(wchar_t);

        // Convert result to A
        const auto converted = ConvertToA(codepage, { targetBuffer.get(), targetWritten });

        // Copy safely to output buffer
        RETURN_IF_FAILED(StringCchCopyNA(target.data(), target.size(), converted.data(), converted.size()));

        // And return the size copied.
        written = converted.size();

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves a command line alias from the global set.
// Arguments:
// - source - The shorthand/alias or source buffer to use in lookup
// - target - The destination/expansion or target buffer we are attempting to retrieve.
// - written - Will specify how many characters were written
// - exeName - The client EXE application attached to the host whose set we should check
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasWImpl(const std::wstring_view source,
                                                        gsl::span<wchar_t> target,
                                                        size_t& written,
                                                        const std::wstring_view exeName) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        HRESULT hr = GetConsoleAliasWImplHelper(source, target, written, exeName);

        if (FAILED(hr))
        {
            written = target.size();
        }

        return hr;
    }
    CATCH_RETURN();
}

// These variables define the separator character and the length of the string.
// They will be used to as the joiner between source and target strings when returning alias data in list form.
static std::wstring aliasesSeparator(L"=");

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - countInUnicode - True for W version (UTF-16 Unicode) calls. False for A version calls (all multibyte formats.)
// - codepage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - bufferRequired - Receives the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT GetConsoleAliasesLengthWImplHelper(const std::wstring_view exeName,
                                                         const bool countInUnicode,
                                                         const UINT codepage,
                                                         size_t& bufferRequired)
{
    // Ensure output variables are initialized
    bufferRequired = 0;

    try
    {
        const std::wstring exeNameString(exeName);

        size_t cchNeeded = 0;

        // Each of the aliases will be made up of the source, a separator, the target, then a null character.
        // They are of the form "Source=Target" when returned.
        size_t const cchNull = 1;
        size_t cchSeperator = aliasesSeparator.size();
        // If we're counting how much multibyte space will be needed, trial convert the separator before we add.
        if (!countInUnicode)
        {
            cchSeperator = GetALengthFromW(codepage, aliasesSeparator);
        }

        // Find without creating.
        auto exeIter = g_aliasData.find(exeNameString);
        if (exeIter != g_aliasData.end())
        {
            auto list = exeIter->second;
            for (auto& pair : list)
            {
                // Alias stores lengths in bytes.
                size_t cchSource = pair.first.size();
                size_t cchTarget = pair.second.size();

                // If we're counting how much multibyte space will be needed, trial convert the source and target strings before we add.
                if (!countInUnicode)
                {
                    cchSource = GetALengthFromW(codepage, pair.first);
                    cchTarget = GetALengthFromW(codepage, pair.second);
                }

                // Accumulate all sizes to the final string count.
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSource, &cchNeeded));
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSeperator, &cchNeeded));
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchTarget, &cchNeeded));
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));
            }
        }

        bufferRequired = cchNeeded;
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - bufferRequired - Receives the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                                size_t& bufferRequired) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    UINT const codepage = gci.CP;

    // Ensure output variables are initialized
    bufferRequired = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    // Convert our input parameters to Unicode
    try
    {
        const auto exeNameW = ConvertToW(codepage, exeName);

        return GetConsoleAliasesLengthWImplHelper(exeNameW, false, codepage, bufferRequired);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the amount of space needed to hold all aliases (source=target pairs) for the given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - bufferRequired - Receives the length of buffer that would be required to retrieve all aliases for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                                size_t& bufferRequired) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return GetConsoleAliasesLengthWImplHelper(exeName, true, 0, bufferRequired);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Clears all aliases on CMD.exe.
void Alias::s_ClearCmdExeAliases()
{
    // find without creating.
    auto exeIter = g_aliasData.find(L"cmd.exe");
    if (exeIter != g_aliasData.end())
    {
        exeIter->second.clear();
    }
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - exeName  - The client EXE application attached to the host whose set we should check
// - aliasBuffer - The target buffer to hold all alias pairs we are trying to retrieve.
//                 Optionally nullopt to retrieve needed space.
// - writtenOrNeeded - Pointer to space that will specify how many characters were written (if buffer is valid)
//                     or how many characters would have been needed (if buffer is nullopt).
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT GetConsoleAliasesWImplHelper(const std::wstring_view exeName,
                                                   std::optional<gsl::span<wchar_t>> aliasBuffer,
                                                   size_t& writtenOrNeeded)
{
    // Ensure output variables are initialized.
    writtenOrNeeded = 0;

    if (aliasBuffer.has_value() && aliasBuffer.value().size() > 0)
    {
        aliasBuffer.value().at(0) = UNICODE_NULL;
    }

    std::wstring exeNameString(exeName);

    LPWSTR AliasesBufferPtrW = aliasBuffer.has_value() ? aliasBuffer.value().data() : nullptr;
    size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

    // Each of the alises will be made up of the source, a separator, the target, then a null character.
    // They are of the form "Source=Target" when returned.
    size_t const cchNull = 1;

    // Find without creating.
    auto exeIter = g_aliasData.find(exeNameString);
    if (exeIter != g_aliasData.end())
    {
        auto list = exeIter->second;
        for (auto& pair : list)
        {
            // Alias stores lengths in bytes.
            size_t const cchSource = pair.first.size();
            size_t const cchTarget = pair.second.size();

            // Add up how many characters we will need for the full alias data.
            size_t cchNeeded = 0;
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSource, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, aliasesSeparator.size(), &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchTarget, &cchNeeded));
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));

            // If we can return the data, attempt to do so until we're done or it overflows.
            // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
            if (aliasBuffer.has_value())
            {
                // Calculate the new final total after we add what we need to see if it will exceed the limit
                size_t cchNewTotal;
                RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));

                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > gsl::narrow<size_t>(aliasBuffer.value().size()));

                size_t cchAliasBufferRemaining;
                RETURN_IF_FAILED(SizeTSub(aliasBuffer.value().size(), cchTotalLength, &cchAliasBufferRemaining));

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, pair.first.data(), cchSource));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, cchSource, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchSource;

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, aliasesSeparator.data(), aliasesSeparator.size()));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, aliasesSeparator.size(), &cchAliasBufferRemaining));
                AliasesBufferPtrW += aliasesSeparator.size();

                RETURN_IF_FAILED(StringCchCopyNW(AliasesBufferPtrW, cchAliasBufferRemaining, pair.second.data(), cchTarget));
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, cchTarget, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchTarget;

                // StringCchCopyNW ensures that the destination string is null terminated, so simply advance the pointer.
                RETURN_IF_FAILED(SizeTSub(cchAliasBufferRemaining, 1, &cchAliasBufferRemaining));
                AliasesBufferPtrW += cchNull;
            }

            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));
        }
    }

    writtenOrNeeded = cchTotalLength;

    return S_OK;
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// - Will convert all input from A to W, call the W version of the function, then convert resulting W to A text and return.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - alias - The target buffer to hold all alias pairs we are trying to retrieve.
// - written - Will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasesAImpl(const std::string_view exeName,
                                                          gsl::span<char> alias,
                                                          size_t& written) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    UINT const codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;

    try
    {
        if (alias.size() > 0)
        {
            alias.at(0) = '\0';
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Convert our input parameters to Unicode.
        const auto exeNameW = ConvertToW(codepage, exeName);
        wistd::unique_ptr<wchar_t[]> pwsExeName;

        // Figure out how big our temporary Unicode buffer must be to retrieve output
        size_t bufferNeeded;
        RETURN_IF_FAILED(GetConsoleAliasesWImplHelper(exeNameW, std::nullopt, bufferNeeded));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == bufferNeeded);

        // Allocate a unicode buffer of the right size.
        std::unique_ptr<wchar_t[]> aliasBuffer = std::make_unique<wchar_t[]>(bufferNeeded);
        RETURN_IF_NULL_ALLOC(aliasBuffer);

        // Call the Unicode version of this method
        size_t bufferWritten;
        RETURN_IF_FAILED(GetConsoleAliasesWImplHelper(exeNameW, gsl::span<wchar_t>(aliasBuffer.get(), bufferNeeded), bufferWritten));

        // Convert result to A
        const auto converted = ConvertToA(codepage, { aliasBuffer.get(), bufferWritten });

        // Copy safely to the output buffer
        // - Aliases are a series of null terminated strings. We cannot use a SafeString function to copy.
        //   So instead, validate and use raw memory copy.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), converted.size() > gsl::narrow<size_t>(alias.size()));
        memcpy_s(alias.data(), alias.size(), converted.data(), converted.size());

        // And return the size copied.
        written = converted.size();

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves all source=target pairs representing alias definitions for a given EXE name
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - alias - The target buffer to hold all alias pairs we are trying to retrieve.
// - written - Will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                          gsl::span<wchar_t> alias,
                                                          size_t& written) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return GetConsoleAliasesWImplHelper(exeName, alias, written);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - countInUnicode - True for W version (UCS-2 Unicode) calls. False for A version calls (all multibyte formats.)
// - codepage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - bufferRequired - Receives the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT GetConsoleAliasExesLengthImplHelper(const bool countInUnicode, const UINT codepage, size_t& bufferRequired)
{
    // Ensure output variables are initialized
    bufferRequired = 0;

    size_t cchNeeded = 0;

    // Each alias exe will be made up of the string payload and a null terminator.
    size_t const cchNull = 1;

    for (auto& pair : g_aliasData)
    {
        size_t cchExe = pair.first.size();

        // If we're counting how much multibyte space will be needed, trial convert the exe string before we add.
        if (!countInUnicode)
        {
            cchExe = GetALengthFromW(codepage, pair.first);
        }

        // Accumulate to total
        RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchExe, &cchNeeded));
        RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchNull, &cchNeeded));
    }

    bufferRequired = cchNeeded;

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// Arguments:
// - bufferRequired - Receives the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept
{
    LockConsole();
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    return GetConsoleAliasExesLengthImplHelper(false, gci.CP, bufferRequired);
}

// Routine Description:
// - Retrieves the amount of space needed to hold all EXE names with aliases defined that are known to the console
// Arguments:
// - bufferRequired - Pointer to receive the length of buffer that would be required to retrieve all relevant EXE names.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    return GetConsoleAliasExesLengthImplHelper(true, 0, bufferRequired);
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - pwsAliasExesBuffer - The target buffer to hold all known EXE names we are trying to retrieve.
//                        Optionally nullopt to retrieve needed space.
// - cchAliasExesBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasExesBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written (if buffer is valid)
//                                        or how many characters would have been needed (if buffer is nullopt).
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT GetConsoleAliasExesWImplHelper(std::optional<gsl::span<wchar_t>> aliasExesBuffer,
                                                     size_t& writtenOrNeeded)
{
    // Ensure output variables are initialized.
    writtenOrNeeded = 0;
    if (aliasExesBuffer.has_value() && aliasExesBuffer.value().size() > 0)
    {
        aliasExesBuffer.value().at(0) = UNICODE_NULL;
    }

    LPWSTR AliasExesBufferPtrW = aliasExesBuffer.has_value() ? aliasExesBuffer.value().data() : nullptr;
    size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

    size_t const cchNull = 1;

    for (auto& pair : g_aliasData)
    {
        // AliasList stores length in bytes. Add 1 for null terminator.
        size_t const cchExe = pair.first.size();

        size_t cchNeeded;
        RETURN_IF_FAILED(SizeTAdd(cchExe, cchNull, &cchNeeded));

        // If we can return the data, attempt to do so until we're done or it overflows.
        // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
        if (aliasExesBuffer.has_value())
        {
            // Calculate the new total length after we add to the buffer
            // Error out early if there is a problem.
            size_t cchNewTotal;
            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > gsl::narrow<size_t>(aliasExesBuffer.value().size()));

            size_t cchRemaining;
            RETURN_IF_FAILED(SizeTSub(aliasExesBuffer.value().size(), cchTotalLength, &cchRemaining));

            RETURN_IF_FAILED(StringCchCopyNW(AliasExesBufferPtrW, cchRemaining, pair.first.data(), cchExe));
            AliasExesBufferPtrW += cchNeeded;
        }

        // Accumulate the total written amount.
        RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));
    }

    writtenOrNeeded = cchTotalLength;

    return S_OK;
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// - Will call the W version of the function and convert all text back to A on returning.
// Arguments:
// - aliasExes - The target buffer to hold all known EXE names we are trying to retrieve.
// - written - Specifies how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesAImpl(gsl::span<char> aliasExes,
                                                            size_t& written) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    UINT const codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;

    try
    {
        if (aliasExes.size() > 0)
        {
            aliasExes.at(0) = '\0';
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Figure our how big our temporary Unicode buffer must be to retrieve output
        size_t bufferNeeded;
        RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(std::nullopt, bufferNeeded));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == bufferNeeded);

        // Allocate a unicode buffer of the right size.
        std::unique_ptr<wchar_t[]> targetBuffer = std::make_unique<wchar_t[]>(bufferNeeded);
        RETURN_IF_NULL_ALLOC(targetBuffer);

        // Call the Unicode version of this method
        size_t bufferWritten;
        RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(gsl::span<wchar_t>(targetBuffer.get(), bufferNeeded), bufferWritten));

        // Convert result to A
        const auto converted = ConvertToA(codepage, { targetBuffer.get(), bufferWritten });

        // Copy safely to the output buffer
        // - AliasExes are a series of null terminated strings. We cannot use a SafeString function to copy.
        //   So instead, validate and use raw memory copy.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), converted.size() > gsl::narrow<size_t>(aliasExes.size()));
        memcpy_s(aliasExes.data(), aliasExes.size(), converted.data(), converted.size());

        // And return the size copied.
        written = converted.size();

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves all EXE names with aliases defined that are known to the console.
// Arguments:
// - pwsAliasExesBuffer - The target buffer to hold all known EXE names we are trying to retrieve.
// - cchAliasExesBufferLength - Length in characters of target buffer. Set to 0 when buffer is nullptr.
// - pcchAliasExesBufferWrittenOrNeeded - Pointer to space that will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesWImpl(gsl::span<wchar_t> aliasExes,
                                                            size_t& written) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return GetConsoleAliasExesWImplHelper(aliasExes, written);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Trims leading spaces off of a string
// Arguments:
// - str - String to trim
void Alias::s_TrimLeadingSpaces(std::wstring& str)
{
    // Erase from the beginning of the string up until the first
    // character found that is not a space.
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(), [](wchar_t ch) { return !std::iswspace(ch); }));
}

// Routine Description:
// - Trims trailing \r\n off of a string
// Arguments:
// - str - String to trim
void Alias::s_TrimTrailingCrLf(std::wstring& str)
{
    const auto trailingCrLfPos = str.find_last_of(UNICODE_CARRIAGERETURN);
    if (std::wstring::npos != trailingCrLfPos)
    {
        str.erase(trailingCrLfPos);
    }
}

// Routine Description:
// - Tokenizes a string into a collection using space as a separator
// Arguments:
// - str - String to tokenize
// Return Value:
// - Collection of tokenized strings
std::deque<std::wstring> Alias::s_Tokenize(const std::wstring& str)
{
    std::deque<std::wstring> result;

    size_t prevIndex = 0;
    auto spaceIndex = str.find(L' ');
    while (std::wstring::npos != spaceIndex)
    {
        const auto length = spaceIndex - prevIndex;

        result.emplace_back(str.substr(prevIndex, length));

        spaceIndex++;
        prevIndex = spaceIndex;

        spaceIndex = str.find(L' ', spaceIndex);
    }

    // Place the final one into the set.
    result.emplace_back(str.substr(prevIndex));

    return result;
}

// Routine Description:
// - Gets just the arguments portion of the command string
//   Specifically, all text after the first space character.
// Arguments:
// - str - String to split into just args
// Return Value:
// - Only the arguments part of the string or empty if there are no arguments.
std::wstring Alias::s_GetArgString(const std::wstring& str)
{
    std::wstring result;
    auto firstSpace = str.find_first_of(L' ');
    if (std::wstring::npos != firstSpace)
    {
        firstSpace++;
        if (firstSpace < str.size())
        {
            result = str.substr(firstSpace);
        }
    }

    return result;
}

// Routine Description:
// - Checks the given character to see if it is a numbered arg replacement macro
//   and replaces it with the counted argument if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// - tokens - Tokens of the original command string. 0 is alias. 1-N are arguments.
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplaceNumberedArgMacro(const wchar_t ch,
                                         std::wstring& appendToStr,
                                         const std::deque<std::wstring>& tokens)
{
    if (ch >= L'1' && ch <= L'9')
    {
        // Numerical macros substitute that numbered argument
        const size_t index = ch - L'0';

        if (index < tokens.size() && index > 0)
        {
            appendToStr.append(tokens[index]);
        }

        return true;
    }

    return false;
}

// Routine Description:
// - Checks the given character to see if it is a wildcard arg replacement macro
//   and replaces it with the entire argument string if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// - fullArgString - All of the arguments as one big string.
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplaceWildcardArgMacro(const wchar_t ch,
                                         std::wstring& appendToStr,
                                         const std::wstring fullArgString)
{
    if (L'*' == ch)
    {
        // Wildcard substitutes all arguments
        appendToStr.append(fullArgString);
        return true;
    }

    return false;
}

// Routine Description:
// - Checks the given character to see if it is an input redirection macro
//   and replaces it with the < redirector if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplaceInputRedirMacro(const wchar_t ch,
                                        std::wstring& appendToStr)
{
    if (L'L' == towupper(ch))
    {
        // L (either case) replaces with input redirector <
        appendToStr.push_back(L'<');
        return true;
    }
    return false;
}

// Routine Description:
// - Checks the given character to see if it is an output redirection macro
//   and replaces it with the > redirector if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplaceOutputRedirMacro(const wchar_t ch,
                                         std::wstring& appendToStr)
{
    if (L'G' == towupper(ch))
    {
        // G (either case) replaces with output redirector >
        appendToStr.push_back(L'>');
        return true;
    }
    return false;
}

// Routine Description:
// - Checks the given character to see if it is a pipe redirection macro
//   and replaces it with the | redirector if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplacePipeRedirMacro(const wchar_t ch,
                                       std::wstring& appendToStr)
{
    if (L'B' == towupper(ch))
    {
        // B (either case) replaces with pipe operator |
        appendToStr.push_back(L'|');
        return true;
    }
    return false;
}

// Routine Description:
// - Checks the given character to see if it is a next command macro
//   and replaces it with CRLF if there is a match
// Arguments:
// - ch - Character to test as a macro
// - appendToStr - Append the macro result here if it matched
// - lineCount - Updates the rolling count of lines if we add a CRLF.
// Return Value:
// - True if we found the macro and appended to the string.
// - False if the given character doesn't match this macro.
bool Alias::s_TryReplaceNextCommandMacro(const wchar_t ch,
                                         std::wstring& appendToStr,
                                         size_t& lineCount)
{
    if (L'T' == towupper(ch))
    {
        // T (either case) inserts a CRLF to chain commands
        s_AppendCrLf(appendToStr, lineCount);
        return true;
    }
    return false;
}

// Routine Description:
// - Appends the system line feed (CRLF) to the given string
// Arguments:
// - appendToStr - Append the system line feed here
// - lineCount - Updates the rolling count of lines if we add a CRLF.
void Alias::s_AppendCrLf(std::wstring& appendToStr,
                         size_t& lineCount)
{
    appendToStr.push_back(L'\r');
    appendToStr.push_back(L'\n');
    lineCount++;
}

// Routine Description:
// - Searches through the given string for macros and replaces them
//   with the matching action
// Arguments:
// - str - On input, the string to search. On output, the string is replaced.
// - tokens - The tokenized command line input. 0 is the alias, 1-N are arguments.
// - fullArgString - Shorthand to 1-N argument string in case of wildcard match.
// Return Value:
// - The number of commands in the final string (line feeds, CRLFs)
size_t Alias::s_ReplaceMacros(std::wstring& str,
                              const std::deque<std::wstring>& tokens,
                              const std::wstring& fullArgString)
{
    size_t lineCount = 0;
    std::wstring finalText;

    // The target text may contain substitution macros indicated by $.
    // Walk through and substitute them as appropriate.
    for (auto ch = str.cbegin(); ch < str.cend(); ch++)
    {
        if (L'$' == *ch)
        {
            // Attempt to read ahead by one character.
            const auto chNext = ch + 1;

            if (chNext < str.cend())
            {
                auto isProcessed = s_TryReplaceNumberedArgMacro(*chNext, finalText, tokens);
                if (!isProcessed)
                {
                    isProcessed = s_TryReplaceWildcardArgMacro(*chNext, finalText, fullArgString);
                }
                if (!isProcessed)
                {
                    isProcessed = s_TryReplaceInputRedirMacro(*chNext, finalText);
                }
                if (!isProcessed)
                {
                    isProcessed = s_TryReplaceOutputRedirMacro(*chNext, finalText);
                }
                if (!isProcessed)
                {
                    isProcessed = s_TryReplacePipeRedirMacro(*chNext, finalText);
                }
                if (!isProcessed)
                {
                    isProcessed = s_TryReplaceNextCommandMacro(*chNext, finalText, lineCount);
                }
                if (!isProcessed)
                {
                    // If nothing matches, just push these two characters in.
                    finalText.push_back(*ch);
                    finalText.push_back(*chNext);
                }

                // Since we read ahead and used that character,
                // advance the iterator one extra to compensate.
                ch++;
            }
            else
            {
                // If no read-ahead, just push this character and be done.
                finalText.push_back(*ch);
            }
        }
        else
        {
            // If it didn't match the macro specifier $, push the character.
            finalText.push_back(*ch);
        }
    }

    // We always terminate with a CRLF to symbolize end of command.
    s_AppendCrLf(finalText, lineCount);

    // Give back the final text and count.
    str.swap(finalText);
    return lineCount;
}

// Routine Description:
// - Takes the source text and searches it for an alias belonging to exe name's list.
// Arguments:
// - sourceText - The string to search for an alias
// - exeName - The name of the EXE that has aliases associated
// - lineCount - Number of lines worth of text processed.
// Return Value:
// - If we found a matching alias, this will be the processed data
//   and lineCount is updated to the new number of lines.
// - If we didn't match and process an alias, return an empty string.
std::wstring Alias::s_MatchAndCopyAlias(const std::wstring& sourceText,
                                        const std::wstring& exeName,
                                        size_t& lineCount)
{
    // Copy source text into a local for manipulation.
    std::wstring sourceCopy(sourceText);

    // Trim trailing \r\n off of sourceCopy if it has one.
    s_TrimTrailingCrLf(sourceCopy);

    // Trim leading spaces off of sourceCopy if it has any.
    s_TrimLeadingSpaces(sourceCopy);

    // Check if we have an EXE in the list that matches the request first.
    auto exeIter = g_aliasData.find(exeName);
    if (exeIter == g_aliasData.end())
    {
        // We found no data for this exe. Give back an empty string.
        return std::wstring();
    }

    auto exeList = exeIter->second;
    if (exeList.size() == 0)
    {
        // If there's no match, give back an empty string.
        return std::wstring();
    }

    // Tokenize the text by spaces
    const auto tokens = s_Tokenize(sourceCopy);

    // If there are no tokens, return an empty string
    if (tokens.size() == 0)
    {
        return std::wstring();
    }

    // Find alias. If there isn't one, return an empty string
    const auto alias = tokens.front();
    const auto aliasIter = exeList.find(alias);
    if (aliasIter == exeList.end())
    {
        // We found no alias pair with this name. Give back an empty string.
        return std::wstring();
    }

    const auto target = aliasIter->second;
    if (target.size() == 0)
    {
        return std::wstring();
    }

    // Get the string of all parameters as a shorthand for $* later.
    const auto allParams = s_GetArgString(sourceCopy);

    // The final text will be the target but with macros replaced.
    std::wstring finalText(target);
    lineCount = s_ReplaceMacros(finalText, tokens, allParams);

    return finalText;
}

// Routine Description:
// - This routine matches the input string with an alias and copies the alias to the input buffer.
// Arguments:
// - pwchSource - string to match
// - cbSource - length of pwchSource in bytes
// - pwchTarget - where to store matched string
// - cbTargetSize - on input, contains size of pwchTarget.
// - cbTargetWritten - On output, contains length of alias stored in pwchTarget.
// - pwchExe - Name of exe that command is associated with to find related aliases
// - cbExe - Length in bytes of exe name
// - LineCount - aliases can contain multiple commands.  $T is the command separator
// Return Value:
// - None. It will just maintain the source as the target if we can't match an alias.
void Alias::s_MatchAndCopyAliasLegacy(_In_reads_bytes_(cbSource) PWCHAR pwchSource,
                                      _In_ size_t cbSource,
                                      _Out_writes_bytes_(cbTargetWritten) PWCHAR pwchTarget,
                                      _In_ const size_t cbTargetSize,
                                      size_t& cbTargetWritten,
                                      const std::wstring& exeName,
                                      DWORD& lines)
{
    try
    {
        std::wstring sourceText(pwchSource, cbSource / sizeof(WCHAR));
        size_t lineCount = lines;

        const auto targetText = s_MatchAndCopyAlias(sourceText, exeName, lineCount);

        // Only return data if the reply was non-empty (we had a match).
        if (!targetText.empty())
        {
            const auto cchTargetSize = cbTargetSize / sizeof(wchar_t);

            // If the target text will fit in the result buffer, fill out the results.
            if (targetText.size() <= cchTargetSize)
            {
                // Non-null terminated copy into memory space
                std::copy_n(targetText.data(), targetText.size(), pwchTarget);

                // Return bytes copied.
                cbTargetWritten = gsl::narrow<ULONG>(targetText.size() * sizeof(wchar_t));

                // Return lines info.
                lines = gsl::narrow<DWORD>(lineCount);
            }
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

#ifdef UNIT_TESTING
void Alias::s_TestAddAlias(std::wstring& exe,
                           std::wstring& alias,
                           std::wstring& target)
{
    g_aliasData[exe][alias] = target;
}

void Alias::s_TestClearAliases()
{
    g_aliasData.clear();
}

#endif
