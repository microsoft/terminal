// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace VTApp
{
    class Program
    {
        static string CSI = ((char)0x1b)+"[";
        static void Main(string[] args)
        {
            Console.WindowHeight = 25;
            Console.BufferHeight = 9000;
            Console.WindowWidth = 80;
            Console.BufferWidth = 80;
            
            Console.WriteLine("VT Tester");

            while (true)
            {
                ConsoleKeyInfo keyInfo = Console.ReadKey(true);

                switch (keyInfo.KeyChar)
                {
                    case '\x1b': // escape
                        // this case is for receiving replies from the console host
                        StringBuilder builder = new StringBuilder();
                        builder.Append(keyInfo.KeyChar);

                        keyInfo = Console.ReadKey(true);

                        // 40-7E are the "dispatch" characters meaning the sequence is done.
                        // 0x5B '[' is expected after the escape. So ignore that. We don't know a of a sequence terminated with it, so it also continues the loop.
                        // keep collecting characters as the "reply" until then
                        while (keyInfo.KeyChar < 0x40 || keyInfo.KeyChar > 0x7E || keyInfo.KeyChar == '[')
                        {
                            builder.Append(keyInfo.KeyChar);

                            keyInfo = Console.ReadKey(true);
                        }
                        builder.Append(keyInfo.KeyChar);

                        Console.Title = string.Format(CultureInfo.InvariantCulture, "Response Received: {0}", builder.ToString());
                        break;
                    case '\x8': // backspace
                        Console.Write('\x8');
                        break;
                    case '\x9': // horizontal tab (tab key)
                        Console.Write('\x9');
                        break;
                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                    case 'P':
                    case 'S':
                    case 'T':
                    case 'L':
                    case 'M':
                        Console.Write(CSI);
                        Console.Write(keyInfo.KeyChar);
                        break;
                    case 'O':
                        Console.Write(CSI);
                        Console.Write("@");
                        break;
                    case 'G':
                        Console.Write(CSI);
                        Console.Write('1');
                        Console.Write('4');
                        Console.Write('G');
                        break;
                    case 'v':
                        Console.Write(CSI);
                        Console.Write('1');
                        Console.Write('4');
                        Console.Write('d');
                        break;
                    case 'H':
                        Console.Write(CSI);
                        Console.Write('5');
                        Console.Write(';');
                        Console.Write('1');
                        Console.Write('H');
                        break;
                    case 'h':
                        Console.Write(CSI);
                        Console.Write('?');
                        Console.Write('2');
                        Console.Write('5');
                        Console.Write('h');
                        break;
                    case 'l':
                        Console.Write(CSI);
                        Console.Write('?');
                        Console.Write('2');
                        Console.Write('5');
                        Console.Write('l');
                        break;
                    case '7':
                        Console.Write((char)0x1b);
                        Console.Write('7');
                        break;
                    case '8':
                        Console.Write((char)0x1b);
                        Console.Write('8');
                        break;
                    case 'y':
                        Console.Write(CSI);
                        Console.Write('s');
                        break;
                    case 'u':
                        Console.Write(CSI);
                        Console.Write('u');
                        break;
                    case '~':
                        // move to top left corner
                        Console.Write(CSI);
                        Console.Write('H');

                        // write out a ton of Zs
                        for (int i = 0; i < 24; i++)
                        {
                            for (int j = 0; j < 80; j++)
                            {
                                Console.Write("Z");
                            }
                        }

                        for (int j = 0; j < 79; j++)
                        {
                            Console.Write("Z");
                        }

                        // move to middle-ish
                        Console.Write(CSI);
                        Console.Write("15");
                        Console.Write(';');
                        Console.Write("15");
                        Console.Write('H');
                        break;
                    case 'J':
                        Console.Write(CSI);
                        Console.Write("J");
                        break;
                    case 'j':
                        Console.Write(CSI);
                        Console.Write("1");
                        Console.Write("J");
                        break;
                    case 'K':
                        Console.Write(CSI);
                        Console.Write("K");
                        break;
                    case 'k':
                        Console.Write(CSI);
                        Console.Write("1");
                        Console.Write("K");
                        break;
                    case '0':
                        Console.Write(CSI);
                        Console.Write("m");
                        break;
                    case '1':
                        Console.Write(CSI);
                        Console.Write("1m");
                        break;
                    case '2':
                        Console.Write(CSI);
                        Console.Write("32m");
                        break;
                    case '3':
                        Console.Write(CSI);
                        Console.Write("33m");
                        break;
                    case '4':
                        Console.Write(CSI);
                        Console.Write("34m");
                        break;
                    case '5':
                        Console.Write(CSI);
                        Console.Write("35m");
                        break;
                    case '6':
                        Console.Write(CSI);
                        Console.Write("36m");
                        break;
                    case '!':
                        Console.Write(CSI);
                        Console.Write("91m");
                        break;
                    case '@':
                        Console.Write(CSI);
                        Console.Write("94m");
                        break;
                    case '#':
                        Console.Write(CSI);
                        Console.Write("96m");
                        break;
                    case '$':
                        Console.Write(CSI);
                        Console.Write("101m");
                        break;
                    case '%':
                        Console.Write(CSI);
                        Console.Write("104m");
                        break;
                    case '^':
                        Console.Write(CSI);
                        Console.Write("106m");
                        break;
                    case 'Q':
                        Console.Write(CSI);
                        Console.Write("40m");
                        break;
                    case 'W':
                        Console.Write(CSI);
                        Console.Write("47m");
                        break;
                    case 'q':
                        Console.Write(CSI);
                        Console.Write("41m");
                        break;
                    case 'w':
                        Console.Write(CSI);
                        Console.Write("43m");
                        break;
                    case 'e':
                        Console.Write(CSI);
                        Console.Write("4m");
                        break;
                    case 'd':
                        Console.Write(CSI);
                        Console.Write("24m");
                        break;
                    case 'r':
                        Console.Write(CSI);
                        Console.Write("7m");
                        break;
                    case 'f':
                        Console.Write(CSI);
                        Console.Write("27m");
                        break;
                    case 'R':
                        Console.Write(CSI);
                        Console.Write("6n");
                        break;
                    case 'c':
                        Console.Write(CSI);
                        Console.Write("0c");
                        break;
                    case '9':
                        Console.Write(CSI);
                        Console.Write("1;37;43;4m");
                        break;
                    case '(':
                        Console.Write(CSI);
                        Console.Write("39m");
                        break;
                    case ')':
                        Console.Write(CSI);
                        Console.Write("49m");
                        break;
                    case '<':
                       Console.Write('\xD'); // carriage return \r
                       break;
                    case '>':
                       Console.Write('\xA'); // line feed/new line \n
                       break;
                    case '`':
                        Console.Write("z");
                        break;
                    case '-':
                        {
                            IntPtr hCon = Pinvoke.GetStdHandle(Pinvoke.STD_OUTPUT_HANDLE);

                            int mode;

                            if (Pinvoke.GetConsoleMode(hCon, out mode))
                            {
                                if ((mode & Pinvoke.ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
                                {
                                    mode &= ~Pinvoke.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                                }
                                else
                                {
                                    mode |= Pinvoke.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                                }

                                Pinvoke.SetConsoleMode(hCon, mode);
                            }
                            break;
                        }
                    case '_':
                        {
                            IntPtr hCon = Pinvoke.GetStdHandle(Pinvoke.STD_INPUT_HANDLE);

                            int mode;
                            if (Pinvoke.GetConsoleMode(hCon, out mode))
                            {
                                if ((mode & Pinvoke.ENABLE_VIRTUAL_TERMINAL_INPUT) != 0)
                                {
                                    mode &= ~Pinvoke.ENABLE_VIRTUAL_TERMINAL_INPUT;
                                }
                                else
                                {
                                    mode |= Pinvoke.ENABLE_VIRTUAL_TERMINAL_INPUT;
                                }
                                mode &= ~Pinvoke.ENABLE_PROCESSED_INPUT;
                                Pinvoke.SetConsoleMode(hCon, mode);
                            }
                            break;
                        }
                    case 's':
                        Console.Write(CSI);
                        Console.Write("5S");
                        break;
                    case 't':
                        Console.Write(CSI);
                        Console.Write("5T");
                        break;
                    case '\'':
                        Console.Write(CSI);
                        Console.Write("3L");
                        break;
                    case '"':
                        Console.Write(CSI);
                        Console.Write("3M");
                        break;
                    case '\\':
                        Console.Write(CSI);
                        Console.Write("?3l");
                        break;
                    case '|':
                        Console.Write(CSI);
                        Console.Write("?3h");
                        break;
                    case '&':
                        Console.Write(CSI);
                        Console.Write("0;0r");
                        break;
                    case '*':
                        Console.Write(CSI + "3;1H");
                        Console.Write(CSI + "1;42m");
                        Console.Write("VVVVVVVVVVVVVVVV");
                        Console.Write(CSI + "4;1H");
                        Console.Write(CSI + "43m");
                        Console.Write("----------------");
                        Console.Write(CSI + "12;1H");
                        Console.Write(CSI + "44m");
                        Console.Write("----------------");
                        Console.Write(CSI + "13;1H");
                        Console.Write(CSI + "45m");
                        Console.Write("^^^^^^^^^^^^^^^^");
                        Console.Write(CSI + "m");
                        Console.Write(CSI + "1m");
                        Console.Write(CSI + "5;2Ha");
                        Console.Write(CSI + "6;3Hb");
                        Console.Write(CSI + "7;4Hc");
                        Console.Write(CSI + "8;5Hd");
                        Console.Write(CSI + "9;6He");
                        Console.Write(CSI + "10;7Hf");
                        Console.Write(CSI + "11;8Hg");
                        Console.Write(CSI + "12;9Hh");
                        Console.Write(CSI + "m");
                        Console.Write(CSI + "4;12r");
                        break;
                    case '{':
                        // move to top left corner
                        Console.Write(CSI);
                        Console.Write('H');

                        // write out a ton of Zs
                        for (int i = 0; i < 24; i++)
                        {
                            for (int j = 0; j < 80; j++)
                            {
                                if (j == 0)
                                    Console.Write(i%10);
                                else
                                    Console.Write("Z");
                            }
                        }

                        for (int j = 0; j < 79; j++)
                        {
                            Console.Write("Z");
                        }

                        // move to middle-ish
                        Console.Write(CSI);
                        Console.Write("15");
                        Console.Write(';');
                        Console.Write("15");
                        Console.Write('H');
                        break;
                    case '=': //Go to the v2 app
                        VTApp2.Program.Main2(args);
                        break;
                }
            }
        }
    }

    public static class Pinvoke
    {
        [DllImport("kernel32.dll")]
        public static extern bool SetConsoleMode(IntPtr hConsoleHandle, int dwMode);

        [DllImport("kernel32.dll")]
        public static extern bool GetConsoleMode(IntPtr hConsoleHandle, out int lpMode);

        public const int ENABLE_PROCESSED_OUTPUT = 0x1;
        public const int ENABLE_WRAP_AT_EOL_OUTPUT = 0x2;
        public const int ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4;

        public const int ENABLE_VIRTUAL_TERMINAL_INPUT = 0x0200;
        public const int ENABLE_PROCESSED_INPUT = 0x0001;
        public const int STD_INPUT_HANDLE = -10;
        public const int STD_OUTPUT_HANDLE = -11;

        [DllImport("kernel32.dll")]
        public static extern IntPtr GetStdHandle(int nStdHandle);
    }
}
