using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ColorTool.ConsoleTargets
{
    class TerminalSchemeConsoleTarget : IConsoleTarget
    {
        public void ApplyColorScheme(ColorScheme colorScheme, bool quietMode)
        {
            Console.WriteLine("Copy and paste the following text into the schemes array of your Windows Terminal profiles.json file to add this color scheme. (Don't forget to add a comma separator after the previous scheme.)");
            Console.WriteLine("{");
            Console.WriteLine($"    \"name\": \"{colorScheme.Name}\",");

            string[] colors =
            {
                "black",
                "blue",
                "green",
                "cyan",
                "red",
                "purple",
                "yellow",
                "white",
                "brightBlack",
                "brightBlue",
                "brightGreen",
                "brightCyan",
                "brightRed",
                "brightPurple",
                "brightYellow",
                "brightWhite",
            };
            Action<uint, string, bool> writeColorItem = (uint color, string name, bool last) =>
            {
                uint r = color & (0x000000ff);
                uint g = (color & (0x0000ff00)) >> 8;
                uint b = (color & (0x00ff0000)) >> 16;
                Console.WriteLine($"    \"{name}\": \"#{r:X2}{g:X2}{b:X2}\"{(last ? "" : ",")}");
            };
            if (colorScheme.ConsoleAttributes.Foreground.HasValue)
            {
                writeColorItem(colorScheme.ConsoleAttributes.Foreground.Value, "foreground", false);
            }
            if (colorScheme.ConsoleAttributes.Background.HasValue)
            {
                writeColorItem(colorScheme.ConsoleAttributes.Background.Value, "background", false);
            }
            for (int i = 0; i < colors.Length; i++)
            {
                writeColorItem(colorScheme.ColorTable[i], colors[i], i == colors.Length - 1);
            }

            Console.WriteLine("}");
        }
    }
}
