// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

import "ITerminalConnection.idl";

namespace Microsoft.Terminal.TerminalConnection
{
    [default_interface]
    runtimeclass ConptyConnection : ITerminalConnection
    {
        ConptyConnection(String cmdline, String startingDirectory, String startingTitle, UInt32 rows, UInt32 columns, Guid guid);
        Guid Guid { get; };
    };

}
