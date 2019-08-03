#include "pch.h"
#include "TerminalConnectionStartupInfo.h"

#include "TerminalConnectionStartupInfo.g.cpp"

winrt::Microsoft::Terminal::TerminalConnection::implementation::TerminalConnectionStartupInfo::TerminalConnectionStartupInfo() :
    _guid(winrt::guid())
{
}

winrt::Microsoft::Terminal::TerminalConnection::implementation::TerminalConnectionStartupInfo::TerminalConnectionStartupInfo(winrt::guid guid) :
    _guid(guid)
{
}
