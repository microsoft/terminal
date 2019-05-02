/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conpropsp.h

Abstract:
- This module is used for reading/writing registry operations
    AND
- This module is used for writing console properties to the link associated
    with a particular console title.

Author(s):
- Michael Niksa (MiNiksa) 23-Jul-2014
- Paul Campbell (PaulCam) 23-Jul-2014
- Mike Griese   (MiGrie)  04-Aug-2015

Revision History:
- From components of srvinit.c
- From miniksa, paulcam's Registry.cpp
--*/

#pragma once

#include "RegistrySerialization.hpp"
#include "ShortcutSerialization.hpp"
#include "TrueTypeFontList.hpp"
