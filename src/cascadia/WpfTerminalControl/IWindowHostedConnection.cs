// <copyright file="IWindowHostedConnection.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;

    /// <summary>
    /// Represents a connection to a terminal backend, generally a pty interface.
    /// </summary>
    public interface IWindowHostedConnection
    {

        void ShowHide(bool show);

        /// <summary>
        /// Inform the connection that the HWND we are hosted in has changed.
        /// </summary>
        /// <param name="newParent">The new HWND's value, as a UInt64.</param>
        void ReparentWindow(ulong newParent);
    }
}
