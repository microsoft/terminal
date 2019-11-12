// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ObjectHandle.h"

#include "..\host\globals.h"
#include "..\host\inputReadHandleData.h"
#include "..\host\input.h"
#include "..\host\screenInfo.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

ConsoleHandleData::ConsoleHandleData(const ACCESS_MASK amAccess,
                                     const ULONG ulShareAccess) :
    _ulHandleType(HandleType::NotReady),
    _amAccess(amAccess),
    _ulShareAccess(ulShareAccess),
    _pvClientPointer(nullptr),
    _pClientInput(nullptr)
{
}

// Routine Description:
// - Closes this handle destroying memory as appropriate and freeing ref counts.
//   Do not use this handle after closing.
ConsoleHandleData::~ConsoleHandleData()
{
    if (_IsInput())
    {
        THROW_IF_FAILED(_CloseInputHandle());
    }
    else if (_IsOutput())
    {
        THROW_IF_FAILED(_CloseOutputHandle());
    }
}

// Routine Description:
// - Holds the client pointer handle for future use after we've determined that
//   we have the privileges to grant it to a particular client.
// - This is separate from construction so this object can help with
//   calculating the access type from the flags, but won't try to
//   clean anything up until the ObjectHeader determines we have rights
//   to use the object (and get it assigned here.)
// Arguments:
// - ulHandleType - specifies that the pointer given is input or output
// - pvClientPointer - pointer to the object that has an ObjectHeader
// Return Value:
// - <none> - But will throw E_NOT_VALID_STATE (for trying to set twice) or
//            E_INVALIDARG (for trying to set the NotReady handle type).
void ConsoleHandleData::Initialize(const ULONG ulHandleType,
                                   PVOID const pvClientPointer)
{
    // This can only be used once and it's an erorr if we try to initialize after it's been done.
    THROW_HR_IF(E_NOT_VALID_STATE, _ulHandleType != HandleType::NotReady);

    // We can't be initialized into the "not ready" state. Only constructed that way.
    THROW_HR_IF(E_INVALIDARG, ulHandleType == HandleType::NotReady);

    _ulHandleType = ulHandleType;
    _pvClientPointer = pvClientPointer;

    if (_IsInput())
    {
        _pClientInput = std::make_unique<INPUT_READ_HANDLE_DATA>();
    }
}

// Routine Description:
// - Checks if this handle represents an input type object.
// Arguments:
// - <none>
// Return Value:
// - True if this handle is for an input object. False otherwise.
bool ConsoleHandleData::_IsInput() const
{
    return WI_IsFlagSet(_ulHandleType, HandleType::Input);
}

// Routine Description:
// - Checks if this handle represents an output type object.
// Arguments:
// - <none>
// Return Value:
// - True if this handle is for an output object. False otherwise.
bool ConsoleHandleData::_IsOutput() const
{
    return WI_IsFlagSet(_ulHandleType, HandleType::Output);
}

// Routine Description:
// - Indicates whether this handle is allowed to be used for reading the underlying object data.
// Arguments:
// - <none>
// Return Value:
// - True if read is permitted. False otherwise.
bool ConsoleHandleData::IsReadAllowed() const
{
    return WI_IsFlagSet(_amAccess, GENERIC_READ);
}

// Routine Description:
// - Indicates whether this handle allows multiple customers to share reading of the underlying object data.
// Arguments:
// - <none>
// Return Value:
// - True if sharing read access is permitted. False otherwise.
bool ConsoleHandleData::IsReadShared() const
{
    return WI_IsFlagSet(_ulShareAccess, FILE_SHARE_READ);
}

// Routine Description:
// - Indicates whether this handle is allowed to be used for writing the underlying object data.
// Arguments:
// - <none>
// Return Value:
// - True if write is permitted. False otherwise.
bool ConsoleHandleData::IsWriteAllowed() const
{
    return WI_IsFlagSet(_amAccess, GENERIC_WRITE);
}

// Routine Description:
// - Indicates whether this handle allows multiple customers to share writing of the underlying object data.
// Arguments:
// - <none>
// Return Value:
// - True if sharing write access is permitted. False otherwise.
bool ConsoleHandleData::IsWriteShared() const
{
    return WI_IsFlagSet(_ulShareAccess, FILE_SHARE_WRITE);
}

// Routine Description:
// - Retieves the properly typed Input Buffer from the Handle.
// Arguments:
// - amRequested - Access that the client would like for manipulating the buffer
// - ppInputBuffer - On success, filled with the referenced Input Buffer object
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT ConsoleHandleData::GetInputBuffer(const ACCESS_MASK amRequested,
                                                        _Outptr_ InputBuffer** const ppInputBuffer) const
{
    *ppInputBuffer = nullptr;

    RETURN_HR_IF(E_ACCESSDENIED, WI_IsAnyFlagClear(_amAccess, amRequested));
    RETURN_HR_IF(E_HANDLE, WI_IsAnyFlagClear(_ulHandleType, HandleType::Input));

    *ppInputBuffer = static_cast<InputBuffer*>(_pvClientPointer);

    return S_OK;
}

// Routine Description:
// - Retieves the properly typed Screen Buffer from the Handle.
// Arguments:
// - amRequested - Access that the client would like for manipulating the buffer
// - ppInputBuffer - On success, filled with the referenced Screen Buffer object
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT ConsoleHandleData::GetScreenBuffer(const ACCESS_MASK amRequested,
                                                         _Outptr_ SCREEN_INFORMATION** const ppScreenInfo) const
{
    *ppScreenInfo = nullptr;

    RETURN_HR_IF(E_ACCESSDENIED, WI_IsAnyFlagClear(_amAccess, amRequested));
    RETURN_HR_IF(E_HANDLE, WI_IsAnyFlagClear(_ulHandleType, HandleType::Output));

    *ppScreenInfo = static_cast<SCREEN_INFORMATION*>(_pvClientPointer);

    return S_OK;
}

// Routine Description:
// - Retrieves the wait queue associated with the given object held by this handle.
// Arguments:
// - ppWaitQueue - On success, filled with a pointer to the desired queue
// Return Value:
// - HRESULT S_OK or E_UNEXPECTED if the handle data structure is in an invalid state.
[[nodiscard]] HRESULT ConsoleHandleData::GetWaitQueue(_Outptr_ ConsoleWaitQueue** const ppWaitQueue) const
{
    CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
    if (_IsInput())
    {
        InputBuffer* const pObj = static_cast<InputBuffer*>(_pvClientPointer);
        *ppWaitQueue = &pObj->WaitQueue;
        return S_OK;
    }
    else if (_IsOutput())
    {
        // TODO MSFT 9405322: shouldn't the output queue be per output object target, not global? https://osgvsowi/9405322
        *ppWaitQueue = &gci.OutputQueue;
        return S_OK;
    }
    else
    {
        return E_UNEXPECTED;
    }
}

// Routine Description:
// - For input buffers only, retrieves an extra handle data structure used to save some information
//   across multiple reads from the same handle.
// Arguments:
// - <none>
// Return Value:
// - Pointer to the input read handle data structure with the aforementioned extra info.
INPUT_READ_HANDLE_DATA* ConsoleHandleData::GetClientInput() const
{
    return _pClientInput.get();
}

// Routine Description:
// - This routine closes an input handle.  It decrements the input buffer's
//   reference count.  If it goes to zero, the buffer is reinitialized.
//   Otherwise, the handle is removed from sharing.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK or suitable error code.
// Note:
// - The console lock must be held when calling this routine.
[[nodiscard]] HRESULT ConsoleHandleData::_CloseInputHandle()
{
    FAIL_FAST_IF(!(_IsInput()));
    InputBuffer* pInputBuffer = static_cast<InputBuffer*>(_pvClientPointer);
    INPUT_READ_HANDLE_DATA* pReadHandleData = GetClientInput();
    pReadHandleData->CompletePending();

    // see if there are any reads waiting for data via this handle.  if
    // there are, wake them up.  there aren't any other outstanding i/o
    // operations via this handle because the console lock is held.

    if (pReadHandleData->GetReadCount() != 0)
    {
        pInputBuffer->WaitQueue.NotifyWaiters(true, WaitTerminationReason::HandleClosing);
    }

    FAIL_FAST_IF(pReadHandleData->GetReadCount() > 0);

    // TODO: MSFT: 9115192 - THIS IS BAD. It should use a destructor.
    LOG_IF_FAILED(pInputBuffer->FreeIoHandle(this));

    if (!pInputBuffer->HasAnyOpenHandles())
    {
        pInputBuffer->ReinitializeInputBuffer();
    }

    return S_OK;
}

// Routine Description:
// - This routine closes an output handle.  It decrements the screen buffer's
//   reference count.  If it goes to zero, the buffer is freed.  Otherwise,
//   the handle is removed from sharing.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK or suitable error code.
// Note:
// - The console lock must be held when calling this routine.
[[nodiscard]] HRESULT ConsoleHandleData::_CloseOutputHandle()
{
    FAIL_FAST_IF(!(_IsOutput()));
    SCREEN_INFORMATION* pScreenInfo = static_cast<SCREEN_INFORMATION*>(_pvClientPointer);

    pScreenInfo = &pScreenInfo->GetMainBuffer();

    // TODO: MSFT: 9115192 - THIS IS BAD. It should use a destructor.
    LOG_IF_FAILED(pScreenInfo->FreeIoHandle(this));
    if (!pScreenInfo->HasAnyOpenHandles())
    {
        SCREEN_INFORMATION::s_RemoveScreenBuffer(pScreenInfo);
    }

    return S_OK;
}
