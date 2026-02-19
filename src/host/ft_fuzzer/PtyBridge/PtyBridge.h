#pragma once

#include <windows.h>

#ifndef PTYB_IMPEXP
#define PTYB_IMPEXP __declspec(dllimport)
#endif

#ifndef PTYB_EXPORT
#ifdef __cplusplus
#define PTYB_EXPORT extern "C" PTYB_IMPEXP
#else
#define PTYB_EXPORT extern PTYB_IMPEXP
#endif
#endif

PTYB_EXPORT int WINAPI PtyRead(int arg0, void * arg1, int arg2);
PTYB_EXPORT int WINAPI PtyWrite(int arg0, void * arg1, int arg2);
PTYB_EXPORT int WINAPI OpenPty(int* pmaster, int* pslave, int width, int height);
PTYB_EXPORT int WINAPI ResizePty(int pmaster, int width, int height);
PTYB_EXPORT int WINAPI SpawnChild(int pmaster, int pslave, const char* child);