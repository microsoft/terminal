//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//
using System;
using System.Diagnostics;
using System.Globalization;
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
        static string RED_KEY = "Red Component";
        static string GREEN_KEY = "Green Component";
        static string BLUE_KEY = "Blue Component";

        static bool parseRgbFromXml(XmlNode components, ref uint rgb)
        {
            int r = -1;
            int g = -1;
            int b = -1;

            foreach (XmlNode c in components.ChildNodes)
            {
                if (c.Name != "key") continue;
                Debug.Assert(c.NextSibling != null, "c.NextSibling != null");
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
            string exeDir = System.IO.Directory.GetParent(System.Reflection.Assembly.GetEntryAssembly().Location).FullName;
            bool found = false;
            string filename = schemeName + ".itermcolors";
            string exeSchemes = exeDir + "/schemes/";
            string cwd = "./";
            string cwdSchemes = "./schemes/";
            // Search order, for argument "name", where 'exe' is the dir of the exe.
            //  1. ./name
            //  2. ./name.itermcolors
            //  3. ./schemes/name
            //  4. ./schemes/name.itermcolors
            //  5. exe/schemes/name
            //  6. exe/schemes/name.itermcolors
            //  7. name (as an absolute path)
            string[] paths = {
                cwd + schemeName,
                cwd + filename,
                cwdSchemes + schemeName,
                cwdSchemes + filename,
                exeSchemes + schemeName,
                exeSchemes + filename,
                schemeName,
            };
            foreach (string path in paths)
            {
                try
                {
                    xmlDoc.Load(path);
                    found = true;
                    break;
                }
                catch (Exception /*e*/)
                {
                    // We can either fail to find the file,
                    //   or fail to parse the XML here.
                }
            }

            if (!found)
            {
                return null;
            }
            return xmlDoc;
        }


        public uint[] ParseScheme(string schemeName)
        {
            XmlDocument xmlDoc = loadXmlScheme(schemeName); // Create an XML document object
            if (xmlDoc == null) return null;
            XmlNode root = xmlDoc.GetElementsByTagName("dict")[0];
            XmlNodeList children = root.ChildNodes;

            uint[] colorTable = new uint[COLOR_TABLE_SIZE];
            int colorsFound = 0;
            bool success = false;
            foreach (XmlNode tableEntry in children)
            {
                if (tableEntry.Name == "key")
                {
                    int index = -1;
                    for (int i = 0; i < COLOR_TABLE_SIZE; i++)
                    {
                        if (PLIST_COLOR_NAMES[i] == tableEntry.InnerText)
                        {
                            index = i;
                            break;
                        }
                    }
                    if (index == -1)
                    {
                        continue;
                    }
                    uint rgb = 0; ;
                    XmlNode components = tableEntry.NextSibling;
                    success = parseRgbFromXml(components, ref rgb);
                    if (!success)
                    {
                        break;
                    }
                    else
                    {
                        colorTable[index] = rgb;
                        colorsFound++;
                    }
                }

            }
            if (colorsFound < COLOR_TABLE_SIZE)
            {
                Console.WriteLine(Resources.InvalidNumberOfColors);
                success = false;
            }
            if (!success)
            {
                return null;
            }
            return colorTable;

        }
    }
}
