// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

HINSTANCE g_hInstance = NULL;
HMODULE g_hModule = NULL;

BOOL APIENTRY DllMain( HMODULE hInstance,
                       DWORD,
                       LPVOID
                     )
{
    g_hModule = g_hInstance = hInstance;
    return TRUE;
}

