/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Abstract:
- This header stores our default namespace guid. This is used in the creation of
  default and in-box dynamic profiles. It also provides a helper function for
  creating a "default" profile. Prior to GH#754, this was used to create the
  cmd, powershell, wsl, pwsh, and azure profiles. Now, this helper is used for
  any of the in-box dynamic profile generators.

Author(s):
- Mike Griese - August 2019
-- */
#pragma once

#include "Profile.h"

// {2bde4a90-d05f-401c-9492-e40884ead1d8}
// uuidv5 properties: name format is UTF-16LE bytes
static constexpr GUID TERMINAL_PROFILE_NAMESPACE_GUID = { 0x2bde4a90, 0xd05f, 0x401c, { 0x94, 0x92, 0xe4, 0x8, 0x84, 0xea, 0xd1, 0xd8 } };

TerminalApp::Profile CreateDefaultProfile(const std::wstring_view name);
