// <copyright file="TerminalOutputEventArgs.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;

    /// <summary>
    /// Event args for output from the terminal backend.
    /// </summary>
    public class TerminalOutputEventArgs : EventArgs
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="TerminalOutputEventArgs"/> class.
        /// </summary>
        /// <param name="data">The backend data associated with the event.</param>
        public TerminalOutputEventArgs(string data)
        {
            this.Data = data;
        }

        /// <summary>
        /// Gets the data sent from the terminal backend.
        /// </summary>
        public string Data { get; }
    }
}
