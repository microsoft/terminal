//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//
using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Linq.Expressions;
using System.Xml;
using static ColorTool.ConsoleAPI;

namespace ColorTool
{
    class XmlSchemeParser : ISchemeParser
    {
        // In Windows Color Table order
        static string[] PLIST_COLOR_NAMES = {
            "Ansi 0 Color",    // DARK_BLACK
            "Ansi 4 Color",  // DARK_BLUE
            "Ansi 2 Color",  // DARK_GREEN
            "Ansi 6 Color",  // DARK_CYAN
            "Ansi 1 Color",  // DARK_RED
            "Ansi 5 Color",  // DARK_MAGENTA
            "Ansi 3 Color",  // DARK_YELLOW
            "Ansi 7 Color",  // DARK_WHITE
            "Ansi 8 Color",  // BRIGHT_BLACK
            "Ansi 12 Color", // BRIGHT_BLUE
            "Ansi 10 Color", // BRIGHT_GREEN
            "Ansi 14 Color", // BRIGHT_CYAN
            "Ansi 9 Color",  // BRIGHT_RED
            "Ansi 13 Color", // BRIGHT_MAGENTA
            "Ansi 11 Color", // BRIGHT_YELLOW
            "Ansi 15 Color" // BRIGHT_WHITE
        };
        static string FG_KEY = "Foreground Color";
        static string BG_KEY = "Background Color";
        static string RED_KEY = "Red Component";
        static string GREEN_KEY = "Green Component";
        static string BLUE_KEY = "Blue Component";

        public string Name => "iTerm Parser";

        static bool parseRgbFromXml(XmlNode components, ref uint rgb)
        {
            int r = -1;
            int g = -1;
            int b = -1;

            foreach (XmlNode c in components.ChildNodes)
            {
                if (c.Name == "key")
                {
                    if (c.InnerText == RED_KEY)
                    {
                        r = (int)(255 * Convert.ToDouble(c.NextSibling.InnerText, CultureInfo.InvariantCulture));
                    }
                    else if (c.InnerText == GREEN_KEY)
                    {
                        g = (int)(255 * Convert.ToDouble(c.NextSibling.InnerText, CultureInfo.InvariantCulture));
                    }
                    else if (c.InnerText == BLUE_KEY)
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


        static XmlDocument loadXmlScheme(string schemeName)
        {
            XmlDocument xmlDoc = new XmlDocument(); // Create an XML document object
            foreach (string path in Scheme.GetSearchPaths(schemeName, ".itermcolors")
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


        public ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            XmlDocument xmlDoc = loadXmlScheme(schemeName); // Create an XML document object
            if (xmlDoc == null) return null;
            XmlNode root = xmlDoc.GetElementsByTagName("dict")[0];
            XmlNodeList children = root.ChildNodes;

            uint[] colorTable = new uint[COLOR_TABLE_SIZE];
            uint? fgColor = null, bgColor = null;
            int colorsFound = 0;
            bool success = false;
            foreach (var tableEntry in children.OfType<XmlNode>().Where(_ => _.Name == "key"))
            {
                uint rgb = 0;
                int index = -1;
                XmlNode components = tableEntry.NextSibling;
                success = parseRgbFromXml(components, ref rgb);
                if (!success) { break; }
                else if (tableEntry.InnerText == FG_KEY) { fgColor = rgb; }
                else if (tableEntry.InnerText == BG_KEY) { bgColor = rgb; }
                else if (-1 != (index = Array.IndexOf(PLIST_COLOR_NAMES, tableEntry.InnerText)))
                { colorTable[index] = rgb; colorsFound++; }
            }
            if (colorsFound < COLOR_TABLE_SIZE)
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

            return new ColorScheme { colorTable = colorTable, foreground = fgColor, background = bgColor };
        }
    }
}
