//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

namespace ColorTool
{
    interface ISchemeParser
    {
        ColorScheme ParseScheme(string schemeName);
    }
}
