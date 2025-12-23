# Adding a Settings Property


using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace MicaAltImplementation
{
    /// <summary>
    /// Mica Alt Material Support for Windows 11
    /// This implementation uses the DWM (Desktop Window Manager) API to apply Mica Alt backdrop
    /// </summary>
    public class MicaAltSupport
    {
        // DWM Attribute constants
        private const int DWMWA_SYSTEMBACKDROP_TYPE = 38;
        private const int DWMWA_MICA_EFFECT = 1029;

        // Backdrop types
        public enum DWM_SYSTEMBACKDROP_TYPE
        {
            DWMSBT_AUTO = 0,           // Let DWM automatically decide
            DWMSBT_NONE = 1,           // No backdrop
            DWMSBT_MAINWINDOW = 2,     // Mica backdrop
            DWMSBT_TRANSIENTWINDOW = 3,// Mica Alt backdrop
            DWMSBT_TABBEDWINDOW = 4    // Tabbed backdrop
        }

        // Import DWM functions
        [DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

        [DllImport("dwmapi.dll")]
        private static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref MARGINS margins);

        [StructLayout(LayoutKind.Sequential)]
        private struct MARGINS
        {
            public int cxLeftWidth;
            public int cxRightWidth;
            public int cyTopHeight;
            public int cyBottomHeight;
        }

        /// <summary>
        /// Apply Mica Alt backdrop to a WPF Window
        /// </summary>
        public static bool ApplyMicaAlt(Window window)
        {
            if (!IsWindows11OrGreater())
            {
                Console.WriteLine("Mica Alt is only supported on Windows 11 or later");
                return false;
            }

            IntPtr hwnd = new WindowInteropHelper(window).Handle;
            if (hwnd == IntPtr.Zero)
            {
                // Window not yet loaded, attach to Loaded event
                window.Loaded += (s, e) => ApplyMicaAltToHwnd(new WindowInteropHelper(window).Handle);
                return true;
            }

            return ApplyMicaAltToHwnd(hwnd);
        }

        /// <summary>
        /// Apply Mica Alt backdrop using window handle
        /// </summary>
        private static bool ApplyMicaAltToHwnd(IntPtr hwnd)
        {
            if (hwnd == IntPtr.Zero)
                return false;

            try
            {
                // Method 1: Use DWMWA_SYSTEMBACKDROP_TYPE (Windows 11 22H2+)
                int backdropType = (int)DWM_SYSTEMBACKDROP_TYPE.DWMSBT_TRANSIENTWINDOW;
                int result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, ref backdropType, sizeof(int));

                if (result == 0)
                {
                    Console.WriteLine("Mica Alt applied successfully using SYSTEMBACKDROP_TYPE");
                    return true;
                }

                // Method 2: Fallback for earlier Windows 11 versions
                int micaEnabled = 1;
                result = DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, ref micaEnabled, sizeof(int));

                if (result == 0)
                {
                    Console.WriteLine("Mica applied successfully using legacy method");
                    return true;
                }

                Console.WriteLine($"Failed to apply Mica Alt. HRESULT: {result}");
                return false;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error applying Mica Alt: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Apply standard Mica backdrop (not Alt)
        /// </summary>
        public static bool ApplyMica(Window window)
        {
            if (!IsWindows11OrGreater())
                return false;

            IntPtr hwnd = new WindowInteropHelper(window).Handle;
            if (hwnd == IntPtr.Zero)
            {
                window.Loaded += (s, e) => ApplyMicaToHwnd(new WindowInteropHelper(window).Handle);
                return true;
            }

            return ApplyMicaToHwnd(hwnd);
        }

        private static bool ApplyMicaToHwnd(IntPtr hwnd)
        {
            if (hwnd == IntPtr.Zero)
                return false;

            int backdropType = (int)DWM_SYSTEMBACKDROP_TYPE.DWMSBT_MAINWINDOW;
            int result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, ref backdropType, sizeof(int));
            return result == 0;
        }

        /// <summary>
        /// Remove backdrop effect
        /// </summary>
        public static bool RemoveBackdrop(Window window)
        {
            IntPtr hwnd = new WindowInteropHelper(window).Handle;
            if (hwnd == IntPtr.Zero)
                return false;

            int backdropType = (int)DWM_SYSTEMBACKDROP_TYPE.DWMSBT_NONE;
            int result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, ref backdropType, sizeof(int));
            return result == 0;
        }

        /// <summary>
        /// Check if running Windows 11 or later
        /// </summary>
        private static bool IsWindows11OrGreater()
        {
            var version = Environment.OSVersion.Version;
            return version.Major >= 10 && version.Build >= 22000;
        }

        /// <summary>
        /// Extend frame into client area for transparent effects
        /// </summary>
        public static void ExtendFrame(Window window)
        {
            IntPtr hwnd = new WindowInteropHelper(window).Handle;
            if (hwnd == IntPtr.Zero)
                return;

            MARGINS margins = new MARGINS
            {
                cxLeftWidth = -1,
                cxRightWidth = -1,
                cyTopHeight = -1,
                cyBottomHeight = -1
            };

            DwmExtendFrameIntoClientArea(hwnd, ref margins);
        }
    }

    // Example Usage in WPF MainWindow
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            
            // Set window properties for best Mica Alt effect
            this.Background = System.Windows.Media.Brushes.Transparent;
            this.AllowsTransparency = false; // Important: should be false for DWM effects
            
            // Apply Mica Alt when window loads
            this.Loaded += MainWindow_Loaded;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // Apply Mica Alt backdrop
            bool success = MicaAltSupport.ApplyMicaAlt(this);
            
            if (success)
            {
                Console.WriteLine("Mica Alt applied successfully!");
            }
            else
            {
                Console.WriteLine("Failed to apply Mica Alt. Check Windows version.");
            }
        }

        // Example: Toggle between Mica and Mica Alt
        private void ToggleBackdrop_Click(object sender, RoutedEventArgs e)
        {
            // Toggle between standard Mica and Mica Alt
            MicaAltSupport.ApplyMica(this);
        }

        private void ApplyMicaAlt_Click(object sender, RoutedEventArgs e)
        {
            MicaAltSupport.ApplyMicaAlt(this);
        }

        private void RemoveBackdrop_Click(object sender, RoutedEventArgs e)
        {
            MicaAltSupport.RemoveBackdrop(this);
        }
    }
}

/* 
 * XAML Configuration for MainWindow:
 * 
 * <Window x:Class="MicaAltImplementation.MainWindow"
 *         xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
 *         xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 *         Title="Mica Alt Demo" 
 *         Height="450" 
 *         Width="800"
 *         Background="Transparent"
 *         WindowStyle="SingleBorderWindow">
 *     <Grid>
 *         <StackPanel VerticalAlignment="Center" HorizontalAlignment="Center">
 *             <TextBlock Text="Mica Alt Material Demo" 
 *                       FontSize="24" 
 *                       Margin="10"/>
 *             <Button Content="Apply Mica Alt" 
 *                    Click="ApplyMicaAlt_Click" 
 *                    Margin="5"/>
 *             <Button Content="Apply Standard Mica" 
 *                    Click="ToggleBackdrop_Click" 
 *                    Margin="5"/>
 *             <Button Content="Remove Backdrop" 
 *                    Click="RemoveBackdrop_Click" 
 *                    Margin="5"/>
 *         </StackPanel>
 *     </Grid>
 * </Window>
 *
 * Requirements:
 * - Windows 11 (Build 22000+)
 * - .NET Framework 4.7.2+ or .NET 6+
 * - Target framework must support Windows 10.0.22000+
 */
