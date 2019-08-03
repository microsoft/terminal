#pragma once

#include "winrt/Microsoft.Terminal.TerminalConnection.h"

winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnectionProvider GetTerminalConnectionProvider();
