// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

using System;

namespace Microsoft.Terminal.Wpf
{
    public class TerminalOutputEventArgs: EventArgs
    {
        public TerminalOutputEventArgs(string data)
        {
            Data = data;
        }

        public string Data { get; }
    }

    public interface ITerminalConnection
    {
        event EventHandler<TerminalOutputEventArgs> TerminalOutput;

        void Start();
        void WriteInput(string data);
        void Resize(uint rows, uint columns);
        void Close();
    };
}
