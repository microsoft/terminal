//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Xml;
using static ColorTool.ConsoleAPI;

namespace ColorTool.SchemeParsers
{
    class XmlSchemeParser : SchemeParserBase
    {
        // In Windows Color Table order
        private static readonly string[] PListColorNames =
        {
            "Ansi 0 Color",  // Dark Black
            "Ansi 4 Color",  // Dark Blue
            "Ansi 2 Color",  // Dark Green
            "Ansi 6 Color",  // Dark Cyan
            "Ansi 1 Color",  // Dark Red
            "Ansi 5 Color",  // Dark Magenta
            "Ansi 3 Color",  // Dark Yellow
            "Ansi 7 Color",  // Dark White
            "Ansi 8 Color",  // Bright Black
            "Ansi 12 Color", // Bright Blue
            "Ansi 10 Color", // Bright Green
            "Ansi 14 Color", // Bright Cyan
            "Ansi 9 Color",  // Bright Red
            "Ansi 13 Color", // Bright Magenta
            "Ansi 11 Color", // Bright Yellow
            "Ansi 15 Color"  // Bright White
        };

        private const string ForegroundKey = "Foreground Color";
        private const string BackgroundKey = "Background Color";
        private const string RedKey = "Red Component";
        private const string GreenKey = "Green Component";
        private const string BlueKey = "Blue Component";

        protected override string FileExtension { get; } = ".itermcolors";

        public override string Name { get; } = "iTerm Parser";

        public override bool CanParse(string schemeName) => 
            string.Equals(Path.GetExtension(schemeName), FileExtension, StringComparison.OrdinalIgnoreCase);

        public override ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            XmlDocument xmlDoc = LoadXmlScheme(schemeName); // Create an XML document object
            if (xmlDoc == null) return null;
            XmlNode root = xmlDoc.GetElementsByTagName("dict")[0];
            XmlNodeList children = root.ChildNodes;

            uint[] colorTable = new uint[ColorTableSize];
            uint? fgColor = null, bgColor = null;
            int colorsFound = 0;
            bool success = false;
            foreach (var tableEntry in children.OfType<XmlNode>().Where(_ => _.Name == "key"))
            {
                uint rgb = 0;
                int index = -1;
                XmlNode components = tableEntry.NextSibling;
                success = ParseRgbFromXml(components, ref rgb);
                if (!success) { break; }
                else if (tableEntry.InnerText == ForegroundKey) { fgColor = rgb; }
                else if (tableEntry.InnerText == BackgroundKey) { bgColor = rgb; }
                else if (-1 != (index = Array.IndexOf(PListColorNames, tableEntry.InnerText)))
                { colorTable[index] = rgb; colorsFound++; }
            }
            if (colorsFound < ColorTableSize)
            {
                if (reportErrors)
                {
                    Console.WriteLine(Resources.InvalidNumberOfColors);
                }
                success = false;
            }
            if (!success)
            {
                return null;
            }

            var consoleAttributes = new ConsoleAttributes(bgColor, fgColor, null, null);
            return new ColorScheme(ExtractSchemeName(schemeName), colorTable, consoleAttributes);
        }

        private static bool ParseRgbFromXml(XmlNode components, ref uint rgb)
        {
            int r = -1;
            int g = -1;
            int b = -1;

            foreach (XmlNode c in components.ChildNodes)
            {
                if (c.Name == "key")
                {
                    if (c.InnerText == RedKey)
                    {
                        r = (int)(255 * Convert.ToDouble(c.NextSibling.InnerText, CultureInfo.InvariantCulture));
                    }
                    else if (c.InnerText == GreenKey)
                    {
                        g = (int)(255 * Convert.ToDouble(c.NextSibling.InnerText, CultureInfo.InvariantCulture));
                    }
                    else if (c.InnerText == BlueKey)
                    {
                        b = (int)(255 * Convert.ToDouble(c.NextSibling.InnerText, CultureInfo.InvariantCulture));
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            if (r < 0 || g < 0 || b < 0)
            {
                Console.WriteLine(Resources.InvalidColor);
                return false;
            }
            rgb = RGB(r, g, b);
            return true;
        }

        private XmlDocument LoadXmlScheme(string schemeName)
        {
            XmlDocument xmlDoc = new XmlDocument(); // Create an XML document object
            foreach (string path in SchemeManager.GetSearchPaths(schemeName, FileExtension)
                                          .Where(File.Exists))
            {
                try
                {
                    xmlDoc.Load(path);
                    return xmlDoc;
                }
                catch (XmlException /*e*/) { /* failed to parse */ }
                catch (IOException /*e*/) { /* failed to find */ }
                catch (UnauthorizedAccessException /*e*/) { /* unauthorized */ }
            }

            return null;
        }
    }
}
