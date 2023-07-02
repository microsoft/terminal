//
//    Copyright (C) Microsoft.  All rights reserved.
//
#ifndef _NTCON_
#define _NTCON_

//
// originally in winconp.h
//
#define CONSOLE_DETACHED_PROCESS ((HANDLE)-1)
#define CONSOLE_NEW_CONSOLE ((HANDLE)-2)
#define CONSOLE_CREATE_NO_WINDOW ((HANDLE)-3)

#define SYSTEM_ROOT_CONSOLE_EVENT 3

#define CONSOLE_READ_NOREMOVE   0x0001
#define CONSOLE_READ_NOWAIT     0x0002

#define CONSOLE_READ_VALID      (CONSOLE_READ_NOREMOVE | CONSOLE_READ_NOWAIT)

#define CONSOLE_GRAPHICS_BUFFER  2

//
// These are flags stored in PEB::ProcessParameters::ConsoleFlags.
//
#define CONSOLE_IGNORE_CTRL_C 0x1


#endif //_NTCON_