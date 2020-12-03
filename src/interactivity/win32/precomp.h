// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma prefast(push)
#pragma prefast(disable : 26071, "Range violation in Intsafe. Not ours.")
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS // Only unsigned intsafe math/casts available without this def
#include <intsafe.h>
#pragma prefast(pop)

#include "../../host/precomp.h"
