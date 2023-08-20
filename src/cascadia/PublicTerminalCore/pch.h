// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN // If this is not defined, windows.h includes commdlg.h which defines FindText globally and conflicts with UIAutomation ITextRangeProvider.
#define NOMCX
#define NOHELP
#define NOCOMM
#endif

#include <LibraryIncludes.h>
#include <UIAutomationCore.h>