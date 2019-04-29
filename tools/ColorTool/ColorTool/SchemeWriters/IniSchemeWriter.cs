//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using ColorTool.SchemeParsers;
using System;
using static ColorTool.ConsoleAPI;

namespace ColorTool.SchemeWriters
{
    class IniSchemeWriter
    {
        public bool ExportCurrentAsIni(string outputPath)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            IntPtr hOut = GetStdOutputHandle();
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (success)
            {
                try
                {
                    // StreamWriter can fail for a variety of file system reasons so catch exceptions and print message.
                    using (System.IO.StreamWriter file = new System.IO.StreamWriter(outputPath))
                    {
                        file.WriteLine("[table]");
                        for (int i = 0; i < 16; i++)
                        {
                            string line = IniSchemeParser.ColorNames[i];
                            line += " = ";
                            uint color = csbiex.ColorTable[i];
                            uint r = color & (0x000000ff);
                            uint g = (color & (0x0000ff00)) >> 8;
                            uint b = (color & (0x00ff0000)) >> 16;
                            line += r + "," + g + "," + b;
                            file.WriteLine(line);
                        }

                        file.WriteLine();
                        file.WriteLine("[screen]");
                        var forgroundIndex = csbiex.wAttributes & 0xF;
                        var backgroundIndex = csbiex.wAttributes >> 4;
                        file.WriteLine($"FOREGROUND = {IniSchemeParser.ColorNames[forgroundIndex]}");
                        file.WriteLine($"BACKGROUND = {IniSchemeParser.ColorNames[backgroundIndex]}");

                        file.WriteLine();
                        file.WriteLine("[popup]");
                        forgroundIndex = csbiex.wPopupAttributes & 0xF;
                        backgroundIndex = csbiex.wPopupAttributes >> 4;
                        file.WriteLine($"FOREGROUND = {IniSchemeParser.ColorNames[forgroundIndex]}");
                        file.WriteLine($"BACKGROUND = {IniSchemeParser.ColorNames[backgroundIndex]}");
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.Message);
                }
            }
            else
            {
                Console.WriteLine("Failed to get console information.");
            }
            return success;
        }
    }
}
