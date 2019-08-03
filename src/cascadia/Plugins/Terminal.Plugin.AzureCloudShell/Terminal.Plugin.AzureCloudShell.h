#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>

// Disable warning about the C-linkage but user defined type. This is consumed by a C++ library so it can be ignored.
#pragma warning(push)
#pragma warning(disable : 4190)
extern "C" {
__declspec(dllexport) winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnectionProvider __cdecl GetConnectionProvider();
}
#pragma warning(pop)
