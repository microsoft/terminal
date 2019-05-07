//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

namespace ColorTool.SchemeParsers
{
    interface ISchemeParser
    {
        string Name { get; }
        bool CanParse(string schemeName);
        ColorScheme ParseScheme(string schemeName, bool reportErrors = false);
    }
}
