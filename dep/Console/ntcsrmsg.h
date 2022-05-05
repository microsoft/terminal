/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
--*/

#pragma once

#include <ntlpcapi.h>

typedef struct _CSR_CAPTURE_HEADER {
} CSR_CAPTURE_HEADER, *PCSR_CAPTURE_HEADER;

typedef struct _CSR_API_MSG {
} CSR_API_MSG, *PCSR_API_MSG;

#define CSR_MAKE_API_NUMBER(DllIndex, ApiIndex) 0
