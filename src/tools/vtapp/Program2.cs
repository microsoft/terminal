// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace VTApp2
{
    class Program
    {

        public static void Main2(string[] args)
        {
            string backupTitle = Console.Title;

            Console.WindowHeight = 25;
            Console.BufferHeight = 9000;
            Console.WindowWidth = 80;
            Console.BufferWidth = 80;

            Console.WriteLine("VT Tester 2, way better than v1");

            IntPtr hCon = Pinvoke.GetStdHandle(Pinvoke.STD_INPUT_HANDLE);
            int mode;
            bool fSuccess = Pinvoke.GetConsoleMode(hCon, out mode);
            if (fSuccess)
            {
                mode &= ~Pinvoke.ENABLE_PROCESSED_INPUT;
                mode |= Pinvoke.ENABLE_VIRTUAL_TERMINAL_INPUT;
                fSuccess = Pinvoke.SetConsoleMode(hCon, mode);
            }
            if (!fSuccess) return;

            while (true)
            {
                ConsoleKeyInfo keyInfo = Console.ReadKey(false);
                switch (keyInfo.KeyChar)
                {
                    case '=':
                    case '>':
                        enableVT();
                        Console.Write((char)0x1b);
                        Console.Write(keyInfo.KeyChar);
                        disableVT();
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
                        enableVT();
                        Console.Write((char)0x1b);
                        Console.Write((char)0x5b);
                        Console.Write(keyInfo.KeyChar);
                        disableVT();
                        break;

                }
            }
        }
        public static void enableVT()
        {
            IntPtr hCon = Pinvoke.GetStdHandle(Pinvoke.STD_OUTPUT_HANDLE);

            int mode;
            bool fSuccess = Pinvoke.GetConsoleMode(hCon, out mode);

            if (fSuccess)
            {
                mode |= Pinvoke.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                fSuccess = Pinvoke.SetConsoleMode(hCon, mode);
            }
        }
        public static void disableVT()
        {
            IntPtr hCon = Pinvoke.GetStdHandle(Pinvoke.STD_OUTPUT_HANDLE);
            int mode;
            bool fSuccess = Pinvoke.GetConsoleMode(hCon, out mode);

            if (fSuccess)
            {
                mode &= ~Pinvoke.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                fSuccess = Pinvoke.SetConsoleMode(hCon, mode);
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
