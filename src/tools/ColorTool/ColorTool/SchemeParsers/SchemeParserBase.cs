using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ColorTool.SchemeParsers
{
    internal abstract class SchemeParserBase : ISchemeParser
    {
        // ISchemeParser elements

        public abstract string Name { get; }

        public abstract bool CanParse(string schemeName);

        public abstract ColorScheme ParseScheme(string schemeName, bool reportErrors = false);

        // Common elements and helpers
        protected abstract string FileExtension { get; }

        protected string ExtractSchemeName(string schemeFileName)
        {
            return schemeFileName.Substring(0, schemeFileName.Length - FileExtension.Length);
        }
    }
}

