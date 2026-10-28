// <copyright file="MainWindow.xaml.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

using Microsoft.Terminal.Wpf;
using System;
using System.Text;
using System.Windows;

namespace WpfTerminalTestNetCore
{
    public class EchoConnection : Microsoft.Terminal.Wpf.ITerminalConnection
    {
        private static (byte r, byte g, byte b) ColorFromHue(float hue)
        {
#if false
            var dr = Math.Abs(hue * 6 - 3) - 1;
            var dg = 2 - Math.Abs(hue * 6 - 2);
            var db = 2 - Math.Abs(hue * 6 - 4);
            return (
                (byte)Math.Clamp(255 * Math.Clamp(dr, 0, 1), 0, 255),
                (byte)Math.Clamp(255 * Math.Clamp(dg, 0, 1), 0, 255),
                (byte)Math.Clamp(255 * Math.Clamp(db, 0, 1), 0, 255)
            );
#endif
            return (0xbe, 0xbe, 0xbe);
        }
        const string ST = "\x1b\\";

        private static string _csi(string s) { return $"\x1b[{s}"; }
        private static string _osc(string s) { return $"\x1b]{s}"; }
        private static string _sgr(string s) { return _csi($"{s}m"); }
        private static string _url(string id, string url) { return _osc($"8;id={id};{url}{ST}"); }
        private static string _urle() { return _osc($"8;;{ST}"); }

        const string vtClear = "\x1b[H\x1b[2J\x1b[3J";
        const string CSI = "\x1b[";
        const string OSC = "\x1b]";
        readonly string funPreamble =
            _sgr("0") + "\r\n" +
            "This is my cool website about terminals\r\n" +
            "Here is a list of cool terminals\r\n" +
            "* Windows Terminal (https://github.com/microsoft/terminal)\r\n" +
            "* This test application!\r\n" +
            "\r\n" +
            CSI + "5m[UNDER CONSTRUCTION]" + CSI + "m\r\n" +
            "\r\n" +
            "You are visitor number " + CSI + "53;4;92m00002" + CSI + "m to my website.\r\n" +
            "\r\n" +
            CSI + "38;5;244m" + CSI + "53m This site is part of the Cool Terminals webring! " + CSI + "55m\r\n" +
            "\r\n" +
            " Visit other sites in the web ring...        \r\n" +
            "\r\n" +
            "       " + CSI + "94m" + _url("prev", "https://terminal.site/?previous=1") + "Previous" + _urle() + CSI + "m     " + CSI + "94m" + OSC + "8;id=rand;https://terminal.site/?random=1" + ST + "Random" + OSC + "8;;" + ST + CSI + "m     " + CSI + "94m" + OSC + "8;id=next;https://terminal.site/?next=1" + ST + "Next" + OSC + "8;;" + ST + CSI + "m \r\n" +
            "       " + CSI + "94m" + _url("prev", "https://terminal.site/?previous=1") + "  Site  " + _urle() + CSI + "m                " + CSI + "94m" + OSC + "8;id=next;https://terminal.site/?next=1" + ST + "Site" + OSC + "8;;" + ST + CSI + "m \r\n" +
            CSI + "4;38;5;244m                                             " + CSI + "m\r\n";

        public event EventHandler<TerminalOutputEventArgs> TerminalOutput;

        public void Resize(uint rows, uint columns)
        {
            return;
        }

        private static String _rainbow(String s) {
            StringBuilder sb = new();
            float h = 0.0F;
            var inc = 1F / s.Length;
            foreach(var c in s) {
                if(c >= 0x20) {
                    var (r, g, b) = ColorFromHue(h);
                    sb.Append($"\x1b[38:2::{r}:{g}:{b}m");
                    h += inc;
                }
                sb.Append(c);
            }
            return sb.ToString();
        }

        private void _preamble()
        {
            _print(vtClear);
            _print(_rainbow("Thanks for visiting my website!\r\n"));
            _print(funPreamble);
            _print("^A: prt esc ^B: sgrmouse ^C: win32im ^D: again!\r\n");
            return;
        }
        public void Start()
        {
            _preamble();
        }

        private bool _escapeMode;
        private bool _mouseMode;
        private bool _win32InputMode;

        public void WriteInput(string data)
        {
            if (data.Length == 0)
            {
                return;
            }

            if (data[0] == '\x01') // ^A
            {
                _escapeMode = !_escapeMode;
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Printable ESC mode: {_escapeMode}\r\n"));
            }
            else if (data[0] == '\x02') // ^B
            {
                _mouseMode = !_mouseMode;
                var decSet = _mouseMode ? "h" : "l";
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"\x1b[?1003{decSet}\x1b[?1006{decSet}"));
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"SGR Mouse mode (1003, 1006): {_mouseMode}\r\n"));
            }
            else if ((data[0] == '\x03') ||
                     (data == "\x1b[67;46;3;1;8;1_")) // ^C
            {
                _win32InputMode = !_win32InputMode;
                var decSet = _win32InputMode ? "h" : "l";
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"\x1b[?9001{decSet}"));
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Win32 input mode: {_win32InputMode}\r\n"));

                // If escape mode isn't currently enabled, turn it on now.
                if (_win32InputMode && !_escapeMode)
                {
                    _escapeMode = true;
                    TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Printable ESC mode: {_escapeMode}\r\n"));
                }
            }
            else if (data[0] == '\x04')
            {
                _preamble();
            }
            else
            {
                // Echo back to the terminal, but make backspace/newline work properly.
                var str = data.Replace("\r", "\r\n").Replace("\x7f", "\x08 \x08");
                _print(str);
            }
        }

        private void _print(string str)
        {
            if (_escapeMode)
            {
                str = str.Replace("\x1b", "\u241b");
            }
            TerminalOutput.Invoke(this, new TerminalOutputEventArgs(str));
        }

        public void Close()
        {
            return;
        }
    }
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            Terminal.Loaded += Terminal_Loaded;
        }

        private void Terminal_Loaded(object sender, RoutedEventArgs e)
        {
            var theme = new TerminalTheme
            {
                DefaultBackground = 0x0c0c0c,
                DefaultForeground = 0xcccccc,
                DefaultSelectionBackground = 0xcccccc,
                CursorStyle = CursorStyle.BlinkingBar,
                // This is Campbell.
                ColorTable = new uint[] { 0x0C0C0C, 0x1F0FC5, 0x0EA113, 0x009CC1, 0xDA3700, 0x981788, 0xDD963A, 0xCCCCCC, 0x767676, 0x5648E7, 0x0CC616, 0xA5F1F9, 0xFF783B, 0x9E00B4, 0xD6D661, 0xF2F2F2 },
            };

            Terminal.Connection = new EchoConnection();
            Terminal.SetTheme(theme, "Cascadia Code", 12);
            Terminal.Focus();
        }
    }
}
