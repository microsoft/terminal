/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ObjectHeader.h

Abstract:
- This file defines the header information to count handles attached to a given object

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

#include "ObjectHandle.h"

class ConsoleObjectHeader
{
public:
    ConsoleObjectHeader();

    // NOTE: This class must have a virtual method for the stored "this" pointers to match what we're actually looking for.
    // If there is no virtual method, we may have the "this" pointer be offset by 8 from the actual object that inherits ConsoleObjectHeader.
    virtual ~ConsoleObjectHeader(){};

    [[nodiscard]] HRESULT AllocateIoHandle(const ConsoleHandleData::HandleType ulHandleType,
                                           const ACCESS_MASK amDesired,
                                           const ULONG ulShareMode,
                                           std::unique_ptr<ConsoleHandleData>& out);

    [[nodiscard]] HRESULT FreeIoHandle(_In_ ConsoleHandleData* const pFree);

    bool HasAnyOpenHandles() const;

    // TODO: MSFT 9355013 come up with a better solution than this. http://osgvsowi/9355013
    // It's currently a "load bearing" piece because things like the renderer expect there to always be a "current screen buffer"
    void IncrementOriginalScreenBuffer();

private:
    ULONG _ulOpenCount;
    ULONG _ulReaderCount;
    ULONG _ulWriterCount;
    ULONG _ulReadShareCount;
    ULONG _ulWriteShareCount;

#ifdef UNIT_TESTING
    friend class ObjectTests;
#endif
};
