#pragma once

#define WIN32_LEAN_AND_MEAN
#define UMDF_USING_NTSTATUS
#define NOMINMAX

#include <ntstatus.h>
#include <Windows.h>
#include <winioctl.h>
#include <winternl.h>

// NOTE: These headers depend on Windows/winternl being included first.
#include <condrv.h>
#include <conmsgl1.h>
#include <conmsgl2.h>
#include <conmsgl3.h>

#include <array>
#include <atomic>
#include <wil/win32_helpers.h>
