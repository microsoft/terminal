using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ColorTool.SchemeParsers
{
    internal abstract class SchemeParserBase : ISchemeParser
    {
        // ISchemeParser elements

        public abstract string Name { get; }

        public abstract ColorScheme ParseScheme(string schemeName, bool reportErrors = false);

        // Common elements and helpers
        public abstract string FileExtension { get; }

        protected string ExtractSchemeName(string schemeFileName) =>
            Path.ChangeExtension(schemeFileName, null);
    }
}

