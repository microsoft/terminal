// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "alias.h"

#include <til/hash.h>

#include "output.h"
#include "handle.h"
#include "misc.h"
#include "../types/inc/convert.hpp"

#include "ApiRoutines.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

struct case_insensitive_hash
{
    using is_transparent = void;

    std::size_t operator()(const std::wstring_view& key) const
    {
        til::hasher h;
        for (const auto& ch : key)
        {
            h.write(::towlower(ch));
        }
        return h.finalize();
    }
};

struct case_insensitive_equality
{
    using is_transparent = void;

    bool operator()(const std::wstring_view& lhs, const std::wstring_view& rhs) const
    {
        return til::compare_ordinal_insensitive(lhs, rhs) == 0;
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

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
                                                 std::optional<std::span<wchar_t>> target,
                                                 size_t& writtenOrNeeded,
                                                 const std::wstring_view exeName)
{
    // Ensure output variables are initialized
    writtenOrNeeded = 0;

    if (target.has_value() && target->size() > 0)
    {
        til::at(*target, 0) = UNICODE_NULL;
    }

    std::wstring exeNameString(exeName);
    std::wstring sourceString(source);

    // For compatibility, return ERROR_GEN_FAILURE for any result where the alias can't be found.
    // We use .find for the iterators then dereference to search without creating entries.
    const auto exeIter = g_aliasData.find(exeNameString);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), exeIter == g_aliasData.end());
    const auto& exeData = exeIter->second;
    const auto sourceIter = exeData.find(sourceString);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), sourceIter == exeData.end());
    const auto& targetString = sourceIter->second;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), targetString.size() == 0);

    // TargetLength is a byte count, convert to characters.
    auto targetSize = targetString.size();
    const size_t cchNull = 1;

    // The total space we need is the length of the string + the null terminator.
    size_t neededSize;
    RETURN_IF_FAILED(SizeTAdd(targetSize, cchNull, &neededSize));

    writtenOrNeeded = neededSize;

    if (target.has_value())
    {
        // if the user didn't give us enough space, return with insufficient buffer code early.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), target->size() < neededSize);

        RETURN_IF_FAILED(StringCchCopyNW(target->data(), target->size(), targetString.data(), targetSize));
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
                                                        std::span<char> target,
                                                        size_t& written,
                                                        const std::string_view exeName) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;
    try
    {
        if (target.size() > 0)
        {
            til::at(target, 0) = ANSI_NULL;
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
        auto targetBuffer = std::make_unique<wchar_t[]>(targetNeeded);
        RETURN_IF_NULL_ALLOC(targetBuffer);

        // Call the Unicode version of this method
        size_t targetWritten;
        RETURN_IF_FAILED(GetConsoleAliasWImplHelper(sourceW,
                                                    std::span<wchar_t>(targetBuffer.get(), targetNeeded),
                                                    targetWritten,
                                                    exeNameW));

        // Set the return size copied to the size given before we attempt to copy.
        // Then multiply by sizeof(wchar_t) due to a long-standing bug that we must preserve for compatibility.
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
                                                        std::span<wchar_t> target,
                                                        size_t& written,
                                                        const std::wstring_view exeName) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        auto hr = GetConsoleAliasWImplHelper(source, target, written, exeName);

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
        const size_t cchNull = 1;
        auto cchSeparator = aliasesSeparator.size();
        // If we're counting how much multibyte space will be needed, trial convert the separator before we add.
        if (!countInUnicode)
        {
            cchSeparator = GetALengthFromW(codepage, aliasesSeparator);
        }

        // Find without creating.
        auto exeIter = g_aliasData.find(exeNameString);
        if (exeIter != g_aliasData.end())
        {
            const auto& list = exeIter->second;
            for (auto& pair : list)
            {
                // Alias stores lengths in bytes.
                auto cchSource = pair.first.size();
                auto cchTarget = pair.second.size();

                // If we're counting how much multibyte space will be needed, trial convert the source and target strings before we add.
                if (!countInUnicode)
                {
                    cchSource = GetALengthFromW(codepage, pair.first);
                    cchTarget = GetALengthFromW(codepage, pair.second);
                }

                // Accumulate all sizes to the final string count.
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSource, &cchNeeded));
                RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchSeparator, &cchNeeded));
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

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
                                                   std::optional<std::span<wchar_t>> aliasBuffer,
                                                   size_t& writtenOrNeeded)
{
    // Ensure output variables are initialized.
    writtenOrNeeded = 0;

    if (aliasBuffer.has_value() && aliasBuffer->size() > 0)
    {
        til::at(*aliasBuffer, 0) = UNICODE_NULL;
    }

    std::wstring exeNameString(exeName);

    auto AliasesBufferPtrW = aliasBuffer.has_value() ? aliasBuffer->data() : nullptr;
    size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

    // Each of the aliases will be made up of the source, a separator, the target, then a null character.
    // They are of the form "Source=Target" when returned.
    const size_t cchNull = 1;

    // Find without creating.
    auto exeIter = g_aliasData.find(exeNameString);
    if (exeIter != g_aliasData.end())
    {
        const auto& list = exeIter->second;
        for (auto& pair : list)
        {
            // Alias stores lengths in bytes.
            const auto cchSource = pair.first.size();
            const auto cchTarget = pair.second.size();

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

                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > aliasBuffer->size());

                size_t cchAliasBufferRemaining;
                RETURN_IF_FAILED(SizeTSub(aliasBuffer->size(), cchTotalLength, &cchAliasBufferRemaining));

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
                                                          std::span<char> alias,
                                                          size_t& written) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;

    try
    {
        if (alias.size() > 0)
        {
            til::at(alias, 0) = '\0';
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
        auto aliasBuffer = std::make_unique<wchar_t[]>(bufferNeeded);
        RETURN_IF_NULL_ALLOC(aliasBuffer);

        // Call the Unicode version of this method
        size_t bufferWritten;
        RETURN_IF_FAILED(GetConsoleAliasesWImplHelper(exeNameW, std::span<wchar_t>(aliasBuffer.get(), bufferNeeded), bufferWritten));

        // Convert result to A
        const auto converted = ConvertToA(codepage, { aliasBuffer.get(), bufferWritten });

        // Copy safely to the output buffer
        // - Aliases are a series of null terminated strings. We cannot use a SafeString function to copy.
        //   So instead, validate and use raw memory copy.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), converted.size() > alias.size());
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
                                                          std::span<wchar_t> alias,
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
    const size_t cchNull = 1;

    for (auto& pair : g_aliasData)
    {
        auto cchExe = pair.first.size();

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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
[[nodiscard]] HRESULT GetConsoleAliasExesWImplHelper(std::optional<std::span<wchar_t>> aliasExesBuffer,
                                                     size_t& writtenOrNeeded)
{
    // Ensure output variables are initialized.
    writtenOrNeeded = 0;
    if (aliasExesBuffer.has_value() && aliasExesBuffer->size() > 0)
    {
        til::at(*aliasExesBuffer, 0) = UNICODE_NULL;
    }

    auto AliasExesBufferPtrW = aliasExesBuffer.has_value() ? aliasExesBuffer->data() : nullptr;
    size_t cchTotalLength = 0; // accumulate the characters we need/have copied as we walk the list

    const size_t cchNull = 1;

    for (auto& pair : g_aliasData)
    {
        // AliasList stores length in bytes. Add 1 for null terminator.
        const auto cchExe = pair.first.size();

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
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > aliasExesBuffer->size());

            size_t cchRemaining;
            RETURN_IF_FAILED(SizeTSub(aliasExesBuffer->size(), cchTotalLength, &cchRemaining));

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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesAImpl(std::span<char> aliasExes,
                                                            size_t& written) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;

    try
    {
        if (aliasExes.size() > 0)
        {
            til::at(aliasExes, 0) = '\0';
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Figure our how big our temporary Unicode buffer must be to retrieve output
        size_t bufferNeeded;
        RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(std::nullopt, bufferNeeded));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == bufferNeeded);

        // Allocate a unicode buffer of the right size.
        auto targetBuffer = std::make_unique<wchar_t[]>(bufferNeeded);
        RETURN_IF_NULL_ALLOC(targetBuffer);

        // Call the Unicode version of this method
        size_t bufferWritten;
        RETURN_IF_FAILED(GetConsoleAliasExesWImplHelper(std::span<wchar_t>(targetBuffer.get(), bufferNeeded), bufferWritten));

        // Convert result to A
        const auto converted = ConvertToA(codepage, { targetBuffer.get(), bufferWritten });

        // Copy safely to the output buffer
        // - AliasExes are a series of null terminated strings. We cannot use a SafeString function to copy.
        //   So instead, validate and use raw memory copy.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), converted.size() > aliasExes.size());
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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleAliasExesWImpl(std::span<wchar_t> aliasExes,
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
// - Takes the source text and searches it for an alias belonging to exe name's list.
// Arguments:
// - sourceText - The string to search for an alias
// - exeName - The name of the EXE that has aliases associated
// - lineCount - Number of lines worth of text processed.
// Return Value:
// - If we found a matching alias, this will be the processed data
//   and lineCount is updated to the new number of lines.
// - If we didn't match and process an alias, return an empty string.
std::wstring Alias::s_MatchAndCopyAlias(std::wstring_view sourceText, std::wstring_view exeName, size_t& lineCount)
{
    // Check if we have an EXE in the list that matches the request first.
    const auto exeIter = g_aliasData.find(exeName);
    if (exeIter == g_aliasData.end())
    {
        // We found no data for this exe.
        return {};
    }

    const auto& exeList = exeIter->second;
    if (exeList.size() == 0)
    {
        return {};
    }

    std::wstring_view args[10];
    size_t argc = 0;

    // Split the source string into whitespace delimited arguments.
    for (size_t argBegIdx = 0; argBegIdx < sourceText.size();)
    {
        // Find the end of the current word (= argument).
        const auto argEndIdx = sourceText.find_first_of(L' ', argBegIdx);
        const auto str = til::safe_slice_abs(sourceText, argBegIdx, argEndIdx);

        // str is empty if the text starting at argBegIdx is whitespace.
        // This can only occur if either the source text starts with whitespace or past
        // an argument there's only whitespace text left until the end of sourceText.
        if (str.empty())
        {
            break;
        }

        args[argc] = str;
        argc++;

        if (argc >= std::size(args))
        {
            break;
        }

        // Find the start of the next word (= argument).
        // If the rest of the text is only whitespace, argBegIdx will be npos
        // and the for() loop condition will make us exit.
        argBegIdx = sourceText.find_first_not_of(L' ', argEndIdx);
    }

    // As mentioned above, argc will be 0 if the source text starts with whitespace or only consists of whitespace.
    if (argc == 0)
    {
        return {};
    }

    // The text up to the first space is the alias name.
    const auto aliasIter = exeList.find(args[0]);
    if (aliasIter == exeList.end())
    {
        return {};
    }

    const auto& target = aliasIter->second;
    if (target.size() == 0)
    {
        return {};
    }

    std::wstring buffer;
    size_t lines = 0;

    for (auto it = target.begin(), end = target.end(); it != end;)
    {
        auto ch = *it++;
        if (ch != L'$' || it == end)
        {
            buffer.push_back(ch);
            continue;
        }

        // $ is our "escape character" and this code handles the escape
        // sequence consisting of a single subsequent character.
        ch = *it++;
        const auto chLower = til::tolower_ascii(ch);
        if (chLower >= L'1' && chLower <= L'9')
        {
            // $1-9 = append the given parameter
            const size_t idx = chLower - L'0';
            if (idx < argc)
            {
                buffer.append(args[idx]);
            }
        }
        else if (chLower == L'*')
        {
            // $* = append all parameters
            if (argc > 1)
            {
                // args[] is an array of slices into the source text. This appends the text
                // starting at first argument up to the end of the source to the buffer.
                buffer.append(args[1].data(), sourceText.data() + sourceText.size());
            }
        }
        else if (chLower == L'l')
        {
            buffer.push_back(L'<');
        }
        else if (chLower == L'g')
        {
            buffer.push_back(L'>');
        }
        else if (chLower == L'b')
        {
            buffer.push_back(L'|');
        }
        else if (chLower == L't')
        {
            buffer.append(L"\r\n");
            lines++;
        }
        else
        {
            buffer.push_back(L'$');
            buffer.push_back(ch);
        }
    }

    buffer.append(L"\r\n");
    lines++;

    lineCount = lines;
    return buffer;
}

void Alias::s_TestAddAlias(std::wstring exe, std::wstring alias, std::wstring target)
{
    g_aliasData[std::move(exe)][std::move(alias)] = std::move(target);
}

void Alias::s_TestClearAliases()
{
    g_aliasData.clear();
}
