#pragma once

#include <winternl.h>

namespace mem
{
    struct Arena;
}

using unique_nthandle = wil::unique_any_handle_null<decltype(&::NtClose), ::NtClose>;

struct ConhostHandle
{
    unique_nthandle reference;
    unique_nthandle connection;
};

ConhostHandle spawn_conhost(mem::Arena& arena, const wchar_t* path);
void check_spawn_conhost_dll(int argc, const wchar_t* argv[]);
HANDLE get_active_connection();
void set_active_connection(HANDLE connection);
