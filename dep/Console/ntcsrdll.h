/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
--*/

#pragma once

#include <ntcsrmsg.h>

NTSTATUS CsrClientCallServer(
    PCSR_API_MSG m,
    PCSR_CAPTURE_HEADER CaptureBuffer OPTIONAL,
    ULONG ApiNumber,
    ULONG ArgLength
);
