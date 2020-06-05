/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    globals.h

Abstract:

    Contains declarations for all globally scoped names in the program.
    This file defines the CBoolean Interface Class.

Author:

Revision History:

Notes:

--*/

#pragma once

//
// SAFECAST(obj, type)
//
// This macro is extremely useful for enforcing strong typechecking on other
// macros.  It generates no code.
//
// Simply insert this macro at the beginning of an expression list for
// each parameter that must be typechecked.  For example, for the
// definition of MYMAX(x, y), where x and y absolutely must be integers,
// use:
//
//   #define MYMAX(x, y)    (SAFECAST(x, int), SAFECAST(y, int), ((x) > (y) ? (x) : (y)))
//
//
#define SAFECAST(_obj, _type) (((_type)(_obj) == (_obj) ? 0 : 0), (_type)(_obj))

//
// Bitfields don't get along too well with bools,
// so here's an easy way to convert them:
//
#define BOOLIFY(expr) (!!(expr))

//
// generic COM stuff
//
#define SafeReleaseClear(punk) \
    {                          \
        if ((punk) != NULL)    \
        {                      \
            (punk)->Release(); \
            (punk) = NULL;     \
        }                      \
    }
