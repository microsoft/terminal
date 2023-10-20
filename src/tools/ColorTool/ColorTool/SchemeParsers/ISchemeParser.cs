//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

namespace ColorTool.SchemeParsers
{
    interface ISchemeParser
    {
        string Name { get; }
        string FileExtension { get; }
        ColorScheme ParseScheme(string schemeName, bool reportErrors = false);
    }
}
