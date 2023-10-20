/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
--*/

#pragma once

#include <ntcsrmsg.h>

typedef enum _USER_API_NUMBER {
    UserpEndTask,
} USER_API_NUMBER, *PUSER_API_NUMBER;

typedef struct _ENDTASKMSG {
    HANDLE ProcessId;
    ULONG ConsoleEventCode;
    ULONG ConsoleFlags;
} ENDTASKMSG, *PENDTASKMSG;

typedef struct _USER_API_MSG {
    union {
        ENDTASKMSG EndTask;
    } u;
} USER_API_MSG, *PUSER_API_MSG;
