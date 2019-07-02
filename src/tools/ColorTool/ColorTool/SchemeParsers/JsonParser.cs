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
    class JsonParser : SchemeParserBase
    {
        protected override string FileExtension { get; } = ".json";
        private static readonly IReadOnlyList<string> ConcfgColorNames = new[]
        {
            "black",        // Dark Black
            "dark_blue",    // Dark Blue
            "dark_green",   // Dark Green
            "dark_cyan",    // Dark Cyan
            "dark_red",     // Dark Red
            "dark_magenta", // Dark Magenta
            "dark_yellow",  // Dark Yellow
            "gray",         // Dark White
            "dark_gray",    // Bright Black
            "blue",         // Bright Blue
            "green",        // Bright Green
            "cyan",         // Bright Cyan
            "red",          // Bright Red
            "magenta",      // Bright Magenta
            "yellow",       // Bright Yellow
            "white"         // Bright White
        };

        public override string Name { get; } = "concfg Parser";

        public override bool CanParse(string schemeName) => 
            string.Equals(Path.GetExtension(schemeName), FileExtension, StringComparison.OrdinalIgnoreCase);

        public override ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            XmlDocument xmlDoc = LoadJsonFile(schemeName);
            if (xmlDoc == null) return null;

            try
            {
                XmlNode root = xmlDoc.DocumentElement;
                XmlNodeList children = root.ChildNodes;
                uint[] colorTable = new uint[ColorTableSize]; ;
                for (int i = 0; i < ColorTableSize; i++)
                {
                    string name = ConcfgColorNames[i];
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
                        var foregroundIndex = (ConcfgColorNames as IList<string>).IndexOf(parts[0]);
                        var backgroundIndex = (ConcfgColorNames as IList<string>).IndexOf(parts[1]);
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
                        var foregroundIndex = (ConcfgColorNames as IList<string>).IndexOf(parts[0]);
                        var backgroundIndex = (ConcfgColorNames as IList<string>).IndexOf(parts[1]);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            screenForeground = colorTable[foregroundIndex];
                            screenBackground = colorTable[backgroundIndex];
                        }
                    }
                }

                var consoleAttributes = new ConsoleAttributes(screenBackground, screenForeground, popupBackground, popupForeground);
                return new ColorScheme(ExtractSchemeName(schemeName), colorTable, consoleAttributes);
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

        private XmlDocument LoadJsonFile(string schemeName)
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
