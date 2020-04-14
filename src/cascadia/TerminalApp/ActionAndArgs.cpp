#include "pch.h"
#include "ActionAndArgs.h"
#include "ActionAndArgs.g.cpp"

// We define everything necessary for the ActionAndArgs class in the header, but
// we still need this file to compile the ActionAndArgs.g.cpp file, and we can't
// just include that file in the header.

namespace winrt::TerminalApp::implementation
{
}
