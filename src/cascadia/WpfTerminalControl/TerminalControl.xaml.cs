// <copyright file="TerminalControl.xaml.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Input;
    using System.Windows.Media;

    /// <summary>
    /// A basic terminal control. This control can receive and render standard VT100 sequences.
    /// </summary>
    public partial class TerminalControl : UserControl
    {
        private int accumulatedDelta = 0;

        /// <summary>
        /// Initializes a new instance of the <see cref="TerminalControl"/> class.
        /// </summary>
        public TerminalControl()
        {
            this.InitializeComponent();

            this.termContainer.TerminalScrolled += this.TermControl_TerminalScrolled;
            this.termContainer.UserScrolled += this.TermControl_UserScrolled;
            this.scrollbar.MouseWheel += this.Scrollbar_MouseWheel;

            this.GotFocus += this.TerminalControl_GotFocus;
        }

        /// <summary>
        /// Gets the character rows available to the terminal.
        /// </summary>
        public int Rows => this.termContainer.Rows;

        /// <summary>
        /// Gets the character columns available to the terminal.
        /// </summary>
        public int Columns => this.termContainer.Columns;

        /// <summary>
        /// Sets the connection to a terminal backend.
        /// </summary>
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
        /// <param name="theme">The color theme to use in the terminal.</param>
        /// <param name="fontFamily">The font family to use in the terminal.</param>
        /// <param name="fontSize">The font size to use in the terminal.</param>
        public void SetTheme(TerminalTheme theme, string fontFamily, short fontSize)
        {
            PresentationSource source = PresentationSource.FromVisual(this);

            if (source == null)
            {
                return;
            }

            this.termContainer.SetTheme(theme, fontFamily, fontSize);
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

        /// <summary>
        /// Resizes the terminal to the specified dimensions.
        /// </summary>
        /// <param name="rendersize">Rendering size for the terminal.</param>
        /// <returns>A tuple of (int, int) representing the number of rows and columns in the terminal.</returns>
        public (int rows, int columns) TriggerResize(Size rendersize)
        {
            return this.termContainer.TriggerResize(rendersize);
        }

        private void TerminalControl_GotFocus(object sender, RoutedEventArgs e)
        {
            e.Handled = true;
            this.termContainer.Focus();
        }

        private void Scrollbar_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            this.TermControl_UserScrolled(sender, e.Delta);
        }

        private void TermControl_UserScrolled(object sender, int delta)
        {
            var lineDelta = 120 / SystemParameters.WheelScrollLines;
            this.accumulatedDelta += delta;

            if (this.accumulatedDelta < lineDelta && this.accumulatedDelta > -lineDelta)
            {
                return;
            }

            this.Dispatcher.InvokeAsync(() =>
            {
                var lines = -this.accumulatedDelta / lineDelta;
                this.scrollbar.Value += lines;
                this.accumulatedDelta = 0;

                this.termContainer.UserScroll((int)this.scrollbar.Value);
            });
        }

        private void TermControl_TerminalScrolled(object sender, (int viewTop, int viewHeight, int bufferSize) e)
        {
            this.Dispatcher.InvokeAsync(() =>
            {
                this.scrollbar.Minimum = 0;
                this.scrollbar.Maximum = e.bufferSize - e.viewHeight;
                this.scrollbar.Value = e.viewTop;
                this.scrollbar.ViewportSize = e.viewHeight;
            });
        }

        private void Scrollbar_Scroll(object sender, System.Windows.Controls.Primitives.ScrollEventArgs e)
        {
            var viewTop = (int)e.NewValue;
            this.termContainer.UserScroll(viewTop);
        }
    }
}
