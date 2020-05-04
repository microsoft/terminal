#pragma once

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemConsoleInformation = 132,
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_CONSOLE_INFORMATION {
    ULONG DriverLoaded : 1;
    ULONG Spare : 31;
} SYSTEM_CONSOLE_INFORMATION, *PSYSTEM_CONSOLE_INFORMATION;
