// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Routine Description:
// - Retrieves the string resource from the current module with the given ID
//   from the resources files. See resource.h and the .rc definitions for valid
//   IDs.
// Arguments:
// - id - Resource ID
// Return Value:
// - String resource retrieved from that ID.
// NOTE: `__declspec(noinline) inline`: make one AND ONLY ONE copy of this
// function, and don't actually inline it
__declspec(noinline) inline std::wstring GetStringResource(const UINT id)
{
    // Calling LoadStringW with a pointer-sized storage and no length will
    // return a read-only pointer directly to the resource data instead of
    // copying it immediately into a buffer.
    LPWSTR readOnlyResource = nullptr;
    const auto length = LoadStringW(wil::GetModuleInstanceHandle(),
                                    id,
                                    reinterpret_cast<LPWSTR>(&readOnlyResource),
                                    0);
    LOG_LAST_ERROR_IF(length == 0);
    // However, the pointer and length given are NOT guaranteed to be
    // zero-terminated and most uses of this data will probably want a
    // zero-terminated string. So we're going to construct and return a
    // std::wstring copy from the pointer/length since those are certainly
    // zero-terminated.
    return { readOnlyResource, gsl::narrow<size_t>(length) };
}
