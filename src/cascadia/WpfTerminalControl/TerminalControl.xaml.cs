using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace Microsoft.Terminal.Wpf
{
    /// <summary>
    /// Interaction logic for UserControl1.xaml
    /// </summary>
    public partial class TerminalControl : UserControl
    {
        private int accumulatedDelta = 0;

        public TerminalControl()
        {
            InitializeComponent();
            this.termContainer.TerminalScrolled += TermControl_TerminalScrolled;
            this.termContainer.UserScrolled += TermControl_UserScrolled;
            this.scrollbar.MouseWheel += Scrollbar_MouseWheel;
        }

        private void Scrollbar_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            this.TermControl_UserScrolled(sender, e.Delta);
        }

        private void TermControl_UserScrolled(object sender, int delta)
        {
            var lineDelta = 120 / SystemParameters.WheelScrollLines;
            this.accumulatedDelta += delta;

            if (accumulatedDelta < lineDelta && accumulatedDelta > -lineDelta)
            {
                return;
            }

            Dispatcher.InvokeAsync(() =>
            {
                var lines = -this.accumulatedDelta / lineDelta;
                this.scrollbar.Value += lines;
                this.accumulatedDelta = 0;
            });
        }

        public ITerminalConnection Connection
        {
            set
            {
                this.termContainer.Connection = value;
            }
        }

        /// <summary>
        /// Sets the theme for the terminal. This includes font family, size, color, as well as background and foreground colors.
        /// </summary>
        public void SetTheme(TerminalTheme theme, string fontFamily, short fontSize)
        {
            PresentationSource source = PresentationSource.FromVisual(this);

            if (source == null)
            {
                return;
            }

            int dpiX;
            dpiX = (int)(96.0 * source.CompositionTarget.TransformToDevice.M11);

            this.termContainer.SetTheme(theme, fontFamily, fontSize, dpiX);
        }

        /// <summary>
        /// Resizes the terminal to the specified rows and columns.
        /// </summary>
        /// <param name="rows">Number of rows to display.</param>
        /// <param name="columns">Number of columns to display.</param>
        public void Resize(uint rows, uint columns)
        {
            this.termContainer.Resize(rows, columns);
        }

        private void TermControl_TerminalScrolled(object sender, (int viewTop, int viewHeight, int bufferSize) e)
        {
            Dispatcher.InvokeAsync(() =>
            {

                this.scrollbar.Minimum = 0;
                this.scrollbar.Maximum = e.bufferSize - e.viewHeight;
                this.scrollbar.Value = e.viewTop;
                this.scrollbar.ViewportSize = e.viewHeight;
            });
        }

        private void Scrollbar_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            var viewTop = (int)this.scrollbar.Value;
            this.termContainer.UserScroll(viewTop);
        }
    }
}
