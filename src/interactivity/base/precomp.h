// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "..\..\host\precomp.h"

#ifdef BUILD_ONECORE_INTERACTIVITY
// From ntdef.h and winnt.h
typedef char CCHAR;
typedef short CSHORT;
typedef ULONG CLONG;

typedef CCHAR* PCCHAR;
typedef CSHORT* PCSHORT;
typedef CLONG* PCLONG;

typedef ULONG LOGICAL;
typedef ULONG* PLOGICAL;
// End ntdef.h and winnt.h

#include <ntlpcapi.h>
#endif

#pragma hdrstop
