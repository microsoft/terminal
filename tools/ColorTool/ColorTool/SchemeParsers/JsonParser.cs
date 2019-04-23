//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Json;
using System.Xml;
using static ColorTool.ConsoleAPI;

namespace ColorTool.SchemeParsers
{
    class JsonParser : ISchemeParser
    {
        private const string FileExtension = ".json";
        private static IReadOnlyList<string> CONCFG_COLOR_NAMES = new[]
        {
            "black",        // DARK_BLACK
            "dark_blue",    // DARK_BLUE
            "dark_green",   // DARK_GREEN
            "dark_cyan",    // DARK_CYAN
            "dark_red",     // DARK_RED
            "dark_magenta", // DARK_MAGENTA
            "dark_yellow",  // DARK_YELLOW
            "gray",         // DARK_WHITE
            "dark_gray",    // BRIGHT_BLACK
            "blue",         // BRIGHT_BLUE
            "green",        // BRIGHT_GREEN
            "cyan",         // BRIGHT_CYAN
            "red",          // BRIGHT_RED
            "magenta",      // BRIGHT_MAGENTA
            "yellow",       // BRIGHT_YELLOW
            "white"         // BRIGHT_WHITE
        };

        public string Name { get; } = "concfg Parser";

        public bool CanParse(string schemeName) => 
            string.Equals(Path.GetExtension(schemeName), FileExtension, StringComparison.OrdinalIgnoreCase);

        public ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            XmlDocument xmlDoc = LoadJsonFile(schemeName);
            if (xmlDoc == null) return null;

            try
            {
                XmlNode root = xmlDoc.DocumentElement;
                XmlNodeList children = root.ChildNodes;
                uint[] colorTable = new uint[COLOR_TABLE_SIZE]; ;
                for (int i = 0; i < COLOR_TABLE_SIZE; i++)
                {
                    string name = CONCFG_COLOR_NAMES[i];
                    var node = children.OfType<XmlNode>().Where(n => n.Name == name).Single();
                    colorTable[i] = ParseColor(node.InnerText);
                }


                uint? popupForeground = null;
                uint? popupBackground = null;

                var popupNode = children.OfType<XmlNode>().Where(n => n.Name == "popup_colors").SingleOrDefault();
                if (popupNode != null)
                {
                    var parts = popupNode.InnerText.Split(',');
                    if (parts.Length == 2)
                    {
                        var foregroundIndex = (CONCFG_COLOR_NAMES as IList<string>).IndexOf(parts[0]);
                        var backgroundIndex = (CONCFG_COLOR_NAMES as IList<string>).IndexOf(parts[1]);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            popupForeground = colorTable[foregroundIndex];
                            popupBackground = colorTable[backgroundIndex];
                        }
                    }
                }

                uint? screenForeground = null;
                uint? screenBackground = null;

                var screenNode = children.OfType<XmlNode>().Where(n => n.Name == "screen_colors").SingleOrDefault();
                if (screenNode != null)
                {
                    var parts = screenNode.InnerText.Split(',');
                    if (parts.Length == 2)
                    {
                        var foregroundIndex = (CONCFG_COLOR_NAMES as IList<string>).IndexOf(parts[0]);
                        var backgroundIndex = (CONCFG_COLOR_NAMES as IList<string>).IndexOf(parts[1]);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            screenForeground = colorTable[foregroundIndex];
                            screenBackground = colorTable[backgroundIndex];
                        }
                    }
                }

                var consoleAttributes = new ConsoleAttributes(screenBackground, screenForeground, popupBackground, popupForeground);
                return new ColorScheme(colorTable, consoleAttributes);
            }
            catch (Exception /*e*/)
            {
                if (reportErrors)
                {
                    Console.WriteLine("failed to load json scheme");
                }

                return null;
            }
        }

        private static uint ParseColor(string arg)
        {
            System.Drawing.Color col = System.Drawing.ColorTranslator.FromHtml(arg);
            return RGB(col.R, col.G, col.B);
        }

        private static XmlDocument LoadJsonFile(string schemeName)
        {
            XmlDocument xmlDoc = new XmlDocument();
            foreach (string path in SchemeManager.GetSearchPaths(schemeName, FileExtension)
                              .Where(File.Exists))
            {
                try
                {
                    var data = File.ReadAllBytes(path);
                    var reader = JsonReaderWriterFactory.CreateJsonReader(data, System.Xml.XmlDictionaryReaderQuotas.Max);
                    xmlDoc.Load(reader);
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
