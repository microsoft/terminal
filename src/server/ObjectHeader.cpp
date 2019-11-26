// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ObjectHeader.h"
#include "ObjectHandle.h"

#include "..\host\inputReadHandleData.h"

ConsoleObjectHeader::ConsoleObjectHeader() :
    _ulOpenCount(0),
    _ulReaderCount(0),
    _ulWriterCount(0),
    _ulReadShareCount(0),
    _ulWriteShareCount(0)
{
}

// Routine Description:
// - This routine allocates an input or output handle from the process's handle table.
// - This routine initializes all non-type specific fields in the handle data structure.
// Arguments:
// - ulHandleType - Flag indicating input or output handle.
// - amDesired - The accesses that will be permitted to this handle after creation
// - ulShareMode - The share states that will be permitted to this handle after creation
// - ppOut - On return, filled with a pointer to the handle data structure. When returned to the API caller, cast to a handle value.
// Return Value:
// - HRESULT S_OK or appropriate error.
// Note:
// TODO: MSFT 614400 - Add concurrency SAL to enforce the lock http://osgvsowi/614400
// - The console lock must be held when calling this routine.  The handle is allocated from the per-process handle table.  Holding the console
//   lock serializes both threads within the calling process and any other process that shares the console.
[[nodiscard]] HRESULT ConsoleObjectHeader::AllocateIoHandle(const ConsoleHandleData::HandleType ulHandleType,
                                                            const ACCESS_MASK amDesired,
                                                            const ULONG ulShareMode,
                                                            std::unique_ptr<ConsoleHandleData>& out)
{
    try
    {
        // Allocate all necessary state.
        std::unique_ptr<ConsoleHandleData> pHandleData = std::make_unique<ConsoleHandleData>(amDesired,
                                                                                             ulShareMode);

        // Check the share mode.
        if (((pHandleData->IsReadAllowed()) && (_ulOpenCount > _ulReadShareCount)) ||
            ((!pHandleData->IsReadShared()) && (_ulReaderCount > 0)) ||
            ((pHandleData->IsWriteAllowed()) && (_ulOpenCount > _ulWriteShareCount)) ||
            ((!pHandleData->IsWriteShared()) && (_ulWriterCount > 0)))
        {
            return HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION);
        }

        // Update share/open counts and store handle information.
        _ulOpenCount++;

        if (pHandleData->IsReadAllowed())
        {
            _ulReaderCount++;
        }

        if (pHandleData->IsReadShared())
        {
            _ulReadShareCount++;
        }

        if (pHandleData->IsWriteAllowed())
        {
            _ulWriterCount++;
        }

        if (pHandleData->IsWriteShared())
        {
            _ulWriteShareCount++;
        }

        // Commit the object into the handle now that we've determined we have the rights to use it and have counted up appropriately.
        // This way, the handle will only try to cleanup and decrement its counts after we've validated rights and incremented.
        pHandleData->Initialize(ulHandleType, this);

        out.swap(pHandleData);
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Frees and decrements ref counts of the handle associated with this object.
// Arguments:
// - pFree - Pointer to the handle data to be freed
// Return Value:
// - HRESULT S_OK or appropriate error.
[[nodiscard]] HRESULT ConsoleObjectHeader::FreeIoHandle(_In_ ConsoleHandleData* const pFree)
{
    // This absolutely should not happen and our state is corrupt/bad if we try to release past 0.
    THROW_HR_IF(E_NOT_VALID_STATE, !(_ulOpenCount > 0));

    _ulOpenCount--;

    if (pFree->IsReadAllowed())
    {
        _ulReaderCount--;
    }

    if (pFree->IsReadShared())
    {
        _ulReadShareCount--;
    }

    if (pFree->IsWriteAllowed())
    {
        _ulWriterCount--;
    }

    if (pFree->IsWriteShared())
    {
        _ulWriteShareCount--;
    }

    return S_OK;
}

// Routine Description:
// - Checks if there are any known open handles connected to this object.
// Arguments:
// - <none>
// Return Value:
// - True if there are any (>0) open handles. False if there are none (0).
bool ConsoleObjectHeader::HasAnyOpenHandles() const
{
    return _ulOpenCount != 0;
}

// Routine Description:
// - Adds a fake reference to the ref counts to ensure the original screen buffer is never destroyed.
// - This is a temporary kludge to be solved in TODO http://osgvsowi/9355013
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConsoleObjectHeader::IncrementOriginalScreenBuffer()
{
    _ulOpenCount++;
    _ulReaderCount++;
    _ulReadShareCount++;
    _ulWriterCount++;
    _ulWriteShareCount++;
}
