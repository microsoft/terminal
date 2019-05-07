// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace Microsoft.Terminal.TerminalConnection
{
    delegate void TerminalOutputEventArgs(String output);
    delegate void TerminalDisconnectedEventArgs();

    interface ITerminalConnection
    {
        event TerminalOutputEventArgs TerminalOutput;
        event TerminalDisconnectedEventArgs TerminalDisconnected;

        void Start();
        void WriteInput(String data);
        void Resize(UInt32 rows, UInt32 columns);
        void Close();
    };

}
