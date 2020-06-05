// <copyright file="ITerminalConnection.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;

    /// <summary>
    /// Represents a connection to a terminal backend, generally a pty interface.
    /// </summary>
    public interface ITerminalConnection
    {
        /// <summary>
        /// Event that should be triggered when the terminal backend has new data for the terminal control.
        /// </summary>
        event EventHandler<TerminalOutputEventArgs> TerminalOutput;

        /// <summary>
        /// Inform the backend that the terminal control is ready to start sending and receiving data.
        /// </summary>
        void Start();

        /// <summary>
        /// Write user input to the backend.
        /// </summary>
        /// <param name="data">The data to be written to the terminal backend.</param>
        void WriteInput(string data);

        /// <summary>
        /// Resize the terminal backend.
        /// </summary>
        /// <param name="rows">The number of rows to resize to.</param>
        /// <param name="columns">The number of columns to resize to.</param>
        void Resize(uint rows, uint columns);

        /// <summary>
        /// Shut down the terminal backend process.
        /// </summary>
        void Close();
    }
}
