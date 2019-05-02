/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- tracing.hpp

Abstract:
- This module is used for recording tracing/debugging information to the telemetry ETW channel
- The data is not automatically broadcast to telemetry backends as it does not set the TELEMETRY keyword.
- NOTE: Many functions in this file appear to be copy/pastes. This is because the TraceLog documentation warns 
        to not be "cute" in trying to reduce its macro usages with variables as it can cause unexpected behavior. 

--*/

#pragma once

#include "telemetry.hpp"
