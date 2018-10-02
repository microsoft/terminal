//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System.Collections.Generic;
using System.IO;

namespace ColorTool
{
    static class Scheme
    {
        public static IEnumerable<string> GetSearchPaths(string schemeName, string extension)
        {
            // Search order, for argument "name", where 'exe' is the dir of the exe.
            //  1. ./name
            //  2. ./name.ext
            //  3. ./schemes/name
            //  4. ./schemes/name.ext
            //  5. exe/schemes/name
            //  6. exe/schemes/name.ext
            //  7. name (as an absolute path)

            string cwd = "./";
            yield return cwd + schemeName;

            string filename = schemeName + extension;
            yield return cwd + filename;

            string cwdSchemes = "./schemes/";
            yield return cwdSchemes + schemeName;
            yield return cwdSchemes + filename;

            string exeDir = Directory.GetParent(System.Reflection.Assembly.GetEntryAssembly().Location).FullName;
            string exeSchemes = exeDir + "/schemes/";
            yield return exeSchemes + schemeName;
            yield return exeSchemes + filename;
            yield return schemeName;
        }
    }
}