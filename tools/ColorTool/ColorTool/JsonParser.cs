using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using static ColorTool.ConsoleAPI;

namespace ColorTool
{
    class JsonParser : ISchemeParser
    {
        static string[] CONCFG_COLOR_NAMES = {
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

        public string Name => "concfg Parser";

        static uint ParseColor(string arg)
        {
            System.Drawing.Color col = System.Drawing.ColorTranslator.FromHtml(arg);
            return RGB(col.R, col.G, col.B);
        }

        static XmlDocument loadJsonFile(string schemeName)
        {
            XmlDocument xmlDoc = new XmlDocument();
            foreach (string path in Scheme.GetSearchPaths(schemeName, ".json")
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

        public ColorScheme ParseScheme(string schemeName, bool reportErrors = true)
        {
            XmlDocument xmlDoc = loadJsonFile(schemeName);
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
                return new ColorScheme { colorTable = colorTable };
            }
            catch (Exception /*e*/)
            {
                if (reportErrors)
                {
                    Console.WriteLine("failes to load json scheme");
                }

                return null;
            }
        }
    }
}
