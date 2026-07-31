// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

// This default no-op implementation lives in its own .obj so that the linker
// can skip it when a test DLL supplies its own definition. The classic linking
// model only pulls in .obj files from a .lib if they resolve an otherwise
// unresolved symbol - and nothing else in the test DLL refers to this file.
// See: https://devblogs.microsoft.com/oldnewthing/20250416-00/?p=111077
extern "C" HKL TestHook_TerminalInput_KeyboardLayout()
{
    return nullptr;
}
