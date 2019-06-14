using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Documents;
using System.Windows.Media;

namespace WpfTerminalControl
{
    public class TerminalSettings
    {
        public Color DefaultForeground { get; set; }

        public Color DefaultBackground { get; set; }

        public CursorStyle CursorShape { get; set; }

        public uint CursorHeight { get; set; }

        public Color CursorColor { get; set; }

        public List<Color> ColorTable { get; set; }
    }

    public enum CursorStyle
    {
        Vintage = 0,
        Bar = 1,
        Underscore = 2,
        FilledBox = 3,
        EmptyBox = 4,
    }
}
