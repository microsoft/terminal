#pragma once

#include "TerminalConnectionStartupInfo.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct TerminalConnectionStartupInfo : TerminalConnectionStartupInfoT<TerminalConnectionStartupInfo>
    {
        TerminalConnectionStartupInfo();
        TerminalConnectionStartupInfo(winrt::guid guid);

        guid Guid()
        {
            return _guid;
        }

        hstring CmdLine()
        {
            return _cmdline;
        }

        void CmdLine(hstring cmdline)
        {
            _cmdline = cmdline;
        }

        hstring StartingDirectory()
        {
            return _startingDirectory;
        }

        void StartingDirectory(hstring startingDirectory)
        {
            _startingDirectory = startingDirectory;
        }

        unsigned int InitialRows()
        {
            return _initialRows;
        }

        void InitialRows(unsigned int initialRows)
        {
            _initialRows = initialRows;
        }

        unsigned int InitialColumns()
        {
            return _initialColumns;
        }

        void InitialColumns(unsigned int initialcolumns)
        {
            _initialColumns = initialcolumns;
        }

    private:
        guid _guid;
        hstring _cmdline;
        hstring _startingDirectory;
        unsigned int _initialRows;
        unsigned int _initialColumns;
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct TerminalConnectionStartupInfo : TerminalConnectionStartupInfoT<TerminalConnectionStartupInfo, implementation::TerminalConnectionStartupInfo>
    {
    };
}
