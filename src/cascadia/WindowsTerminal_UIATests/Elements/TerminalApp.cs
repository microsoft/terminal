//----------------------------------------------------------------------------------------------------------------------
// <copyright file="TerminalApp.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Helper and wrapper for generating the base test application and its UI Root node.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace WindowsTerminal.UIA.Tests.Elements
{
    using System.Collections.Generic;
    using System;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Threading;

    using WindowsTerminal.UIA.Tests.Common;
    using WindowsTerminal.UIA.Tests.Common.NativeMethods;

    using OpenQA.Selenium.Remote;
    using OpenQA.Selenium.Appium;
    using OpenQA.Selenium.Appium.iOS;
    using OpenQA.Selenium.Interactions;

    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using System.Runtime.InteropServices;
    using System.Security.Principal;
    using OpenQA.Selenium;
    using System.Windows.Automation;

    public class TerminalApp : IDisposable
    {
        protected const string AppDriverUrl = "http://127.0.0.1:4723";
        private const uint MouseEventLeftDown = 0x0002;
        private const uint MouseEventLeftUp = 0x0004;
        private const uint KeyEventKeyUp = 0x0002;
        private const byte VirtualKeyControl = 0x11;
        private const byte VirtualKeyShift = 0x10;
        private const int InputMouse = 0;
        private const int InputKeyboard = 1;

        private IntPtr job;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool SetCursorPos(int x, int y);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint SendInput(uint numberOfInputs, INPUT[] inputs, int sizeOfInputStructure);

        [StructLayout(LayoutKind.Sequential)]
        private struct INPUT
        {
            public int type;
            public INPUTUNION U;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct INPUTUNION
        {
            [FieldOffset(0)]
            public MOUSEINPUT mi;

            [FieldOffset(0)]
            public KEYBDINPUT ki;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        public IOSDriver<IOSElement> Session { get; private set; }
        public Actions Actions { get; private set; }
        public AppiumWebElement UIRoot { get; private set; }

        private bool isDisposed = false;

        private TestContext context;
        private string _windowTitleToFind;
        private string _dragEventLogPath;
        private readonly HashSet<int> _trackedProcessIds = new HashSet<int>();
        private readonly Dictionary<IntPtr, IOSDriver<IOSElement>> _windowSessions = new Dictionary<IntPtr, IOSDriver<IOSElement>>();
        private readonly List<string> _scheduledTaskNames = new List<string>();
        private readonly List<string> _temporaryLaunchArtifacts = new List<string>();
        private IOSDriver<IOSElement> _desktopSession;

        public sealed class TopLevelWindow
        {
            public TopLevelWindow(IntPtr handle, string title)
            {
                Handle = handle;
                Title = title;
                RefreshBounds();
            }

            public IntPtr Handle { get; }

            public string Title { get; }

            public User32.RECT Bounds { get; private set; }

            internal IOSDriver<IOSElement> Session { get; set; }

            internal AppiumWebElement Root { get; set; }

            public int Width => Bounds.right - Bounds.left;

            public int Height => Bounds.bottom - Bounds.top;

            public void RefreshBounds()
            {
                NativeMethods.Win32BoolHelper(User32.GetWindowRect(Handle, out var rect), $"Get bounds for top-level window '{Title}'.");
                Bounds = rect;
            }
        }

        public string ContentPath { get; private set; }
        public string GetFullTestContentPath(string filename)
        {
            return Path.GetFullPath(Path.Combine(ContentPath, filename));
        }

        public TerminalApp(TestContext context, string shellToLaunch = "powershell.exe", string launchArgs = null, string windowTitleToFind = "WindowsTerminal.UIA.Tests")
        {
            this.context = context;
            _windowTitleToFind = windowTitleToFind;

            // If running locally, set WTPath to where we can find a loose
            // deployment of Windows Terminal. That means you'll need to build
            // the Terminal appx, then use
            // New-UnpackagedTerminalDistribution.ps1 to build an unpackaged
            // layout that can successfully launch. Then, point the tests at
            // that WindowsTerminal.exe like so:
            //
            //   te.exe WindowsTerminal.UIA.Tests.dll /p:WTPath=C:\the\path\to\the\unpackaged\layout\WindowsTerminal.exe
            //
            // On the build machines, the scripts lay it out at the terminal-0.0.1.0\ subfolder of the test deployment directory
            string path = Path.GetFullPath(Path.Combine(context.TestDeploymentDir, @"terminal-0.0.1.0\WindowsTerminal.exe"));
            if (context.Properties.Contains("WTPath"))
            {
                path = (string)context.Properties["WTPath"];
            }
            Log.Comment($"Windows Terminal will be launched from '{path}'");

            // Same goes for the content directory. Set WTTestContent for where the content files are
            // for running tests.
            // On the build machines, the scripts lay it out at the content\ subfolder.
            ContentPath = @"content";
            if (context.Properties.Contains("WTTestContent"))
            {
                ContentPath = (string)context.Properties["WTTestContent"];
            }
            Log.Comment($"Test Content will be loaded from '{Path.GetFullPath(ContentPath)}'");

            this.CreateProcess(path, shellToLaunch, launchArgs);
        }

        ~TerminalApp()
        {
            this.Dispose(false);
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        public AppiumWebElement GetRoot()
        {
            return this.UIRoot;
        }

        public TopLevelWindow FindTopLevelWindowByName(string name, TopLevelWindow excludedWindow = null)
        {
            var matches = new List<TopLevelWindow>();
            var excludedHandle = excludedWindow?.Handle ?? IntPtr.Zero;

            User32.EnumWindows((hWnd, _) =>
            {
                if (hWnd == excludedHandle || !User32.IsWindowVisible(hWnd))
                {
                    return true;
                }

                var title = _GetWindowText(hWnd);
                if (!string.Equals(title, name, StringComparison.Ordinal))
                {
                    return true;
                }

                matches.Add(new TopLevelWindow(hWnd, title));
                return true;
            }, IntPtr.Zero);

            var windowMatch = matches.OrderByDescending(window => window.Width * window.Height).FirstOrDefault();
            if (windowMatch != null)
            {
                _AttachToWindow(windowMatch);
            }

            return windowMatch;
        }

        public TopLevelWindow WaitForTopLevelWindowByName(string name, TopLevelWindow excludedWindow = null, int timeoutMs = 15000)
        {
            var remaining = timeoutMs;
            while (remaining >= 0)
            {
                var window = FindTopLevelWindowByName(name, excludedWindow);
                if (window != null)
                {
                    return window;
                }

                Thread.Sleep(250);
                remaining -= 250;
            }

            _LogVisibleWindows(name);
            throw new InvalidOperationException($"Timed out waiting for top-level window '{name}'.");
        }

        public TopLevelWindow TryWaitForTopLevelWindowByName(string name, TopLevelWindow excludedWindow = null, int timeoutMs = 15000)
        {
            var remaining = timeoutMs;
            while (remaining >= 0)
            {
                var window = FindTopLevelWindowByName(name, excludedWindow);
                if (window != null)
                {
                    return window;
                }

                Thread.Sleep(250);
                remaining -= 250;
            }

            return null;
        }

        public void ActivateWindow(TopLevelWindow window)
        {
            if (!User32.SetForegroundWindow(window.Handle))
            {
                Log.Comment($"SetForegroundWindow returned false for top-level window '{window.Title}' (0x{window.Handle.ToInt64():x}); continuing with existing attachment.");
            }
            Thread.Sleep(250);
            window.RefreshBounds();
            _AttachToWindow(window);
            Session = window.Session;
            UIRoot = window.Root;
        }

        public void ArrangeWindowOnPrimaryMonitor(TopLevelWindow window, int slotIndex, int slotCount = 2)
        {
            Verify.IsNotNull(window);
            Verify.IsTrue(slotCount > 0, "Expected at least one monitor slot.");
            Verify.IsTrue(slotIndex >= 0 && slotIndex < slotCount, $"Expected slot index {slotIndex} to be within {slotCount} slots.");

            const int margin = 24;
            const int top = 120;
            var primaryWidth = User32.GetSystemMetrics(User32.SM_CXSCREEN);
            var primaryHeight = User32.GetSystemMetrics(User32.SM_CYSCREEN);

            Verify.IsTrue(primaryWidth > 0, "Expected a valid primary monitor width.");
            Verify.IsTrue(primaryHeight > 0, "Expected a valid primary monitor height.");

            var availableWidth = primaryWidth - ((slotCount + 1) * margin);
            var targetWidth = Math.Max(720, availableWidth / slotCount);
            var maxHeight = Math.Max(480, primaryHeight - top - margin);
            var targetHeight = Math.Min(window.Height, maxHeight);
            var targetX = margin + ((targetWidth + margin) * slotIndex);

            NativeMethods.Win32BoolHelper(
                User32.SetWindowPos(window.Handle, IntPtr.Zero, targetX, top, targetWidth, targetHeight, User32.SWP_NOZORDER | User32.SWP_SHOWWINDOW),
                $"Arrange top-level window '{window.Title}' in primary monitor slot {slotIndex}.");

            Thread.Sleep(400);
            window.RefreshBounds();
        }

        public AppiumWebElement FindElementByName(TopLevelWindow window, string name)
        {
            window.RefreshBounds();

            _AttachToWindow(window);

            var matches = window.Session.FindElementsByName(name)
                                        .OfType<AppiumWebElement>()
                                        .Where(_IsValidElement);
            if (ReferenceEquals(window.Session, _desktopSession))
            {
                matches = matches.Where(element => _IsElementWithinWindow(element, window));
            }

            return matches.OrderByDescending(element => element.Size.Width * element.Size.Height)
                          .ThenBy(element => element.Location.X)
                          .ThenBy(element => element.Location.Y)
                          .First();
        }

        public AppiumWebElement FindTabElementByName(TopLevelWindow window, string name)
        {
            window.RefreshBounds();

            _AttachToWindow(window);

            var matches = window.Session.FindElementsByName(name)
                                        .OfType<AppiumWebElement>()
                                        .Where(_IsValidElement);
            if (ReferenceEquals(window.Session, _desktopSession))
            {
                matches = matches.Where(element => _IsElementWithinWindow(element, window));
            }

            return matches.Where(element => element.Size.Height <= 80 &&
                                            element.Size.Width < window.Width &&
                                            element.Location.Y < window.Bounds.top + 96)
                          .OrderBy(element => element.Location.X)
                          .ThenBy(element => element.Location.Y)
                          .First();
        }

        public bool HasElementByName(TopLevelWindow window, string name)
        {
            window.RefreshBounds();

            _AttachToWindow(window);

            var matches = window.Session.FindElementsByName(name)
                                        .OfType<AppiumWebElement>()
                                        .Where(_IsValidElement);
            if (ReferenceEquals(window.Session, _desktopSession))
            {
                matches = matches.Where(element => _IsElementWithinWindow(element, window));
            }

            return matches.Any();
        }

        public void LogWindowDetails(string label, TopLevelWindow window)
        {
            window.RefreshBounds();
            Log.Comment(
                $"{label}: title='{window.Title}', handle=0x{window.Handle.ToInt64():x}, bounds=({window.Bounds.left},{window.Bounds.top})-({window.Bounds.right},{window.Bounds.bottom})");
        }

        public void LogElementDetails(string label, TopLevelWindow window, AppiumWebElement element)
        {
            window.RefreshBounds();
            Log.Comment(
                $"{label}: " +
                $"element(Name='{_TryGetAttribute(element, "Name")}', Class='{_TryGetAttribute(element, "ClassName")}', Handle='{_TryGetAttribute(element, "NativeWindowHandle")}', Loc=({element.Location.X},{element.Location.Y}), Size=({element.Size.Width},{element.Size.Height}), Id='{element.Id}'); " +
                $"windowHandle=0x{window.Handle.ToInt64():x}, windowBounds=({window.Bounds.left},{window.Bounds.top})-({window.Bounds.right},{window.Bounds.bottom})");
        }

        public void LogElementAncestors(string label, AppiumWebElement element, int maxDepth = 6)
        {
            var current = element;
            for (var depth = 0; depth < maxDepth && current != null; depth++)
            {
                Log.Comment(
                    $"{label}[{depth}]: " +
                    $"Name='{_TryGetAttribute(current, "Name")}', Class='{_TryGetAttribute(current, "ClassName")}', ControlType='{_TryGetAttribute(current, "ControlType")}', " +
                    $"Loc=({current.Location.X},{current.Location.Y}), Size=({current.Size.Width},{current.Size.Height}), Id='{current.Id}'");

                AppiumWebElement parent = null;
                try
                {
                    parent = current.FindElementByXPath("..");
                }
                catch
                {
                    break;
                }

                if (parent == null || parent.Id == current.Id)
                {
                    break;
                }

                current = parent;
            }
        }

        private static string _TryGetAttribute(AppiumWebElement element, string attributeName)
        {
            try
            {
                return element?.GetAttribute(attributeName);
            }
            catch
            {
                return null;
            }
        }

        private static bool _IsValidElement(AppiumWebElement element)
        {
            return element != null && !string.IsNullOrEmpty(element.Id);
        }

        private static bool _IsElementWithinWindow(AppiumWebElement element, TopLevelWindow window)
        {
            var centerX = element.Location.X + (element.Size.Width / 2);
            var centerY = element.Location.Y + (element.Size.Height / 2);
            return centerX >= window.Bounds.left &&
                   centerX < window.Bounds.right &&
                   centerY >= window.Bounds.top &&
                   centerY < window.Bounds.bottom;
        }

        private static string _GetWindowText(IntPtr hWnd)
        {
            var length = User32.GetWindowTextLength(hWnd);
            if (length <= 0)
            {
                return string.Empty;
            }

            var buffer = new System.Text.StringBuilder(length + 1);
            User32.GetWindowText(hWnd, buffer, buffer.Capacity);
            return buffer.ToString();
        }

        private void _AttachToWindow(TopLevelWindow window)
        {
            if (window == null)
            {
                return;
            }

            if (window.Session != null && window.Root != null)
            {
                return;
            }

            if (_windowSessions.TryGetValue(window.Handle, out var existingSession))
            {
                try
                {
                    window.Session = existingSession;
                    try
                    {
                        window.Root = existingSession.FindElementByXPath("/*");
                    }
                    catch
                    {
                        window.Root = existingSession.FindElementByXPath("//*");
                    }

                    return;
                }
                catch (Exception ex)
                {
                    Log.Comment($"Reused WinAppDriver session for window 0x{window.Handle.ToInt64():x} ('{window.Title}') became invalid: {ex.Message}");
                    try
                    {
                        existingSession.Quit();
                    }
                    catch
                    {
                    }

                    _windowSessions.Remove(window.Handle);
                    window.Session = null;
                    window.Root = null;
                }
            }

            var capabilities = new DesiredCapabilities();
            capabilities.SetCapability("appTopLevelWindow", window.Handle.ToInt64().ToString("x"));

            try
            {
                var windowSession = new IOSDriver<IOSElement>(new Uri(AppDriverUrl), capabilities);
                Verify.IsNotNull(windowSession, $"Attach WinAppDriver session to top-level window '{window.Title}'.");

                AppiumWebElement rootElement;
                try
                {
                    rootElement = windowSession.FindElementByXPath("/*");
                }
                catch
                {
                    rootElement = windowSession.FindElementByXPath("//*");
                }

                window.Session = windowSession;
                window.Root = rootElement;
                _windowSessions[window.Handle] = windowSession;
            }
            catch
            {
                Log.Comment($"Falling back to desktop session for window 0x{window.Handle.ToInt64():x} ('{window.Title}') because appTopLevelWindow attach failed.");
                window.Session = _desktopSession;
                if (UIRoot != null)
                {
                    window.Root = UIRoot;
                }
                else
                {
                    try
                    {
                        window.Root = _desktopSession?.FindElementByXPath("/*");
                    }
                    catch
                    {
                        try
                        {
                            window.Root = _desktopSession?.FindElementByXPath("//*");
                        }
                        catch
                        {
                            window.Root = null;
                        }
                    }
                }
            }
        }

        private void _LogVisibleWindows(string targetName)
        {
            var visibleTitles = new List<string>();
            User32.EnumWindows((hWnd, _) =>
            {
                if (!User32.IsWindowVisible(hWnd))
                {
                    return true;
                }

                var title = _GetWindowText(hWnd);
                if (string.IsNullOrWhiteSpace(title))
                {
                    return true;
                }

                if (title.IndexOf(targetName, StringComparison.OrdinalIgnoreCase) >= 0 ||
                    title.IndexOf(_windowTitleToFind, StringComparison.OrdinalIgnoreCase) >= 0 ||
                    title.IndexOf("DragTab", StringComparison.OrdinalIgnoreCase) >= 0 ||
                    title.IndexOf("Terminal", StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    visibleTitles.Add($"0x{hWnd.ToInt64():x}: {title}");
                }

                return true;
            }, IntPtr.Zero);

            Log.Comment($"Visible top-level windows near '{targetName}': {visibleTitles.Count}");
            foreach (var title in visibleTitles.Take(20))
            {
                Log.Comment(title);
            }
        }

        private static AppiumWebElement _LiftToWindowRoot(AppiumWebElement element)
        {
            if (element == null)
            {
                return null;
            }

            var current = element;
            var processId = _TryGetAttribute(current, "ProcessId");
            for (var i = 0; i < 20; ++i)
            {
                AppiumWebElement parent = null;
                try
                {
                    parent = current.FindElementByXPath("..");
                }
                catch
                {
                    break;
                }

                if (parent == null || parent.Id == current.Id)
                {
                    break;
                }

                if (!string.IsNullOrEmpty(processId))
                {
                    var parentPid = _TryGetAttribute(parent, "ProcessId");
                    if (!string.Equals(parentPid, processId, StringComparison.Ordinal))
                    {
                        break;
                    }
                }

                current = parent;
            }

            return current;
        }

        public void CtrlClick(AppiumWebElement element)
        {
            _ModifiedClick(element, VirtualKeyControl);
        }

        public void CtrlClickTab(TopLevelWindow window, string name)
        {
            _ModifiedNativeTabClick(window, name, VirtualKeyControl);
        }

        public void ShiftClick(AppiumWebElement element)
        {
            _ModifiedClick(element, VirtualKeyShift);
        }

        public void ShiftClickTab(TopLevelWindow window, string name)
        {
            _ModifiedNativeTabClick(window, name, VirtualKeyShift);
        }

        public void ClickTab(TopLevelWindow window, string name)
        {
            _NativeTabClick(window, name);
        }

        public void SelectTabRangeForTesting(TopLevelWindow window, int startIndex, int endIndex)
        {
            var result = User32.SendMessage(window.Handle, User32.WindowMessages.CM_UIA_SELECT_TAB_RANGE, startIndex, new IntPtr(endIndex));
            Log.Comment($"SelectTabRangeForTesting: window=0x{window.Handle.ToInt64():x}, start={startIndex}, end={endIndex}, result={result}");
            Verify.IsTrue(result != IntPtr.Zero, $"Select tab range {startIndex}-{endIndex} for '{window.Title}'.");
            Thread.Sleep(200);
        }

        public void LogNativeTabSelectionState(TopLevelWindow window, string name)
        {
            var element = _FindNativeTabElement(window, name);
            var isSelected = false;
            if (element.TryGetCurrentPattern(SelectionItemPattern.Pattern, out var selectionPattern))
            {
                isSelected = ((SelectionItemPattern)selectionPattern).Current.IsSelected;
            }

            var rect = element.Current.BoundingRectangle;
            Log.Comment($"Native UIA tab '{name}' selection: Selected={isSelected}, Focused={element.Current.HasKeyboardFocus}, Bounds=({rect.Left},{rect.Top})-({rect.Right},{rect.Bottom})");
        }

        public void DragByOffset(AppiumWebElement source, int offsetX, int offsetY)
        {
            var (sourceCenterX, sourceCenterY) = _GetScreenCenter(source);
            var targetX = sourceCenterX + offsetX;
            var targetY = sourceCenterY + offsetY;

            Log.Comment($"Native drag from ({sourceCenterX}, {sourceCenterY}) to ({targetX}, {targetY})");

            _MoveMouseToElementOffset(source, 0, 0);
            Thread.Sleep(100);

            Session.Mouse.MouseDown(null);
            Thread.Sleep(250);

            _MoveCursorSmoothly(source, 0, 0, 36, 0, 8, 25);
            Thread.Sleep(200);

            _MoveCursorSmoothly(source, 36, 0, 84, 0, 10, 25);
            Thread.Sleep(250);

            _MoveCursorSmoothly(source, 84, 0, offsetX, offsetY, 36, 20);

            Thread.Sleep(700);
            Session.Mouse.MouseUp(null);
            Thread.Sleep(400);
        }

        public void DragToElement(AppiumWebElement source, AppiumWebElement target, int targetOffsetX = 0, int targetOffsetY = 0)
        {
            var (sourceCenterX, sourceCenterY) = _GetScreenCenter(source);
            var (targetCenterX, targetCenterY) = _GetScreenCenter(target);
            targetCenterX += targetOffsetX;
            targetCenterY += targetOffsetY;
            DragByOffset(source, targetCenterX - sourceCenterX, targetCenterY - sourceCenterY);
        }

        private void _ModifiedClick(AppiumWebElement element, byte modifierKey)
        {
            var targetX = element.Location.X + Math.Min(16, Math.Max(4, element.Size.Width / 8));
            var targetY = element.Location.Y + (element.Size.Height / 2);
            _ClickScreenPoint(targetX, targetY, modifierKey, "modified click");
        }

        private static INPUT _CreateMouseInput(uint flags)
        {
            return new INPUT
            {
                type = InputMouse,
                U = new INPUTUNION
                {
                    mi = new MOUSEINPUT
                    {
                        dwFlags = flags,
                    },
                },
            };
        }

        private static AutomationElement _FindNativeTabElement(TopLevelWindow window, string name)
        {
            var windowElement = AutomationElement.FromHandle(window.Handle);
            Verify.IsNotNull(windowElement, $"Find native UIA root for top-level window '{window.Title}'.");

            var conditions = new AndCondition(
                new PropertyCondition(AutomationElement.NameProperty, name),
                new PropertyCondition(AutomationElement.ControlTypeProperty, ControlType.TabItem));
            var matches = windowElement.FindAll(TreeScope.Descendants, conditions);
            Verify.IsTrue(matches.Count > 0, $"Find native tab item '{name}' in window '{window.Title}'.");

            return matches
                .OfType<AutomationElement>()
                .Where(element => !element.Current.BoundingRectangle.IsEmpty)
                .OrderBy(element => element.Current.BoundingRectangle.Left)
                .First();
        }

        private void _ModifiedNativeTabClick(TopLevelWindow window, string name, byte modifierKey)
        {
            var element = _FindNativeTabElement(window, name);
            var rect = element.Current.BoundingRectangle;
            var targetX = (int)rect.Left + Math.Min(16, Math.Max(4, (int)rect.Width / 8));
            var targetY = (int)(rect.Top + (rect.Height / 2));
            Log.Comment($"Native UIA tab '{name}' for modified click: Class='{element.Current.ClassName}', ControlType='{element.Current.ControlType.ProgrammaticName}', Bounds=({rect.Left},{rect.Top})-({rect.Right},{rect.Bottom})");
            _ClickScreenPoint(targetX, targetY, modifierKey, $"native tab '{name}' modified click", window.Handle);
        }

        private void _NativeTabClick(TopLevelWindow window, string name)
        {
            var element = _FindNativeTabElement(window, name);
            var rect = element.Current.BoundingRectangle;
            var targetX = (int)rect.Left + Math.Min(16, Math.Max(4, (int)rect.Width / 8));
            var targetY = (int)(rect.Top + (rect.Height / 2));
            Log.Comment($"Native UIA tab '{name}' for click: Class='{element.Current.ClassName}', ControlType='{element.Current.ControlType.ProgrammaticName}', Bounds=({rect.Left},{rect.Top})-({rect.Right},{rect.Bottom})");
            _ClickScreenPoint(targetX, targetY, null, $"native tab '{name}' click", window.Handle);
        }

        private void _ClickScreenPoint(int targetX, int targetY, byte? modifierKey, string description, IntPtr targetWindow = default)
        {
            if (IsRunningAsAdmin())
            {
                _ClickScreenPointLimited(targetX, targetY, modifierKey, description, targetWindow);
                return;
            }

            NativeMethods.Win32BoolHelper(SetCursorPos(targetX, targetY), $"Move cursor to {description} at ({targetX}, {targetY}).");
            Thread.Sleep(100);

            var inputs = new List<INPUT>();
            if (modifierKey.HasValue)
            {
                inputs.Add(_CreateKeyboardInput(modifierKey.Value, false));
            }

            inputs.Add(_CreateMouseInput(MouseEventLeftDown));
            inputs.Add(_CreateMouseInput(MouseEventLeftUp));

            if (modifierKey.HasValue)
            {
                inputs.Add(_CreateKeyboardInput(modifierKey.Value, true));
            }

            var sent = SendInput((uint)inputs.Count, inputs.ToArray(), Marshal.SizeOf<INPUT>());
            Verify.AreEqual((uint)inputs.Count, sent, $"Send {description} input.");
            Thread.Sleep(200);
        }

        private void _ClickScreenPointLimited(int targetX, int targetY, byte? modifierKey, string description, IntPtr targetWindow)
        {
            var taskName = $"WindowsTerminal-UIA-Input-{Process.GetCurrentProcess().Id}-{Guid.NewGuid():N}";
            var scriptPath = Path.Combine(Path.GetTempPath(), $"{taskName}.ps1");
            var launcherPath = Path.Combine(Path.GetTempPath(), $"{taskName}.cmd");
            var resultPath = Path.Combine(Path.GetTempPath(), $"{taskName}.result");
            var modifierValue = modifierKey.HasValue ? modifierKey.Value.ToString(CultureInfo.InvariantCulture) : "-1";
            var targetWindowValue = targetWindow == IntPtr.Zero ? "0" : targetWindow.ToInt64().ToString(CultureInfo.InvariantCulture);
            var escapedResultPath = resultPath.Replace("'", "''");

            var scriptContents = string.Join(Environment.NewLine, new[]
            {
                "$ErrorActionPreference = 'Stop'",
                "try {",
                "Add-Type @'",
                "using System;",
                "using System.Text;",
                "using System.Runtime.InteropServices;",
                "public static class Native {",
                "    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);",
                "    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x; public int y; }",
                "    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left; public int top; public int right; public int bottom; }",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern bool SetCursorPos(int x, int y);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern bool SetForegroundWindow(IntPtr hWnd);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern bool ScreenToClient(IntPtr hWnd, ref POINT lpPoint);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern IntPtr ChildWindowFromPointEx(IntPtr hWndParent, POINT pt, uint flags);",
                "    [DllImport(\"user32.dll\", CharSet=CharSet.Unicode, SetLastError=true)] public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);",
                "    [DllImport(\"user32.dll\", SetLastError=true)] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);",
                "    public static string GetClassNameString(IntPtr hWnd) { var sb = new StringBuilder(256); return GetClassName(hWnd, sb, sb.Capacity) > 0 ? sb.ToString() : string.Empty; }",
                "    private static bool Contains(RECT rect, int x, int y) { return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom; }",
                "    public static IntPtr ResolveInputWindow(IntPtr topWindow, int screenX, int screenY) {",
                "        IntPtr inputSite = IntPtr.Zero;",
                "        IntPtr bridge = IntPtr.Zero;",
                "        EnumChildWindows(topWindow, (child, lParam) => {",
                "            RECT rect;",
                "            if (GetWindowRect(child, out rect) && Contains(rect, screenX, screenY)) {",
                "                var className = GetClassNameString(child);",
                "                if (string.Equals(className, \"Windows.UI.Input.InputSite.WindowClass\", StringComparison.Ordinal)) { inputSite = child; return false; }",
                "                if (bridge == IntPtr.Zero && string.Equals(className, \"Windows.UI.Composition.DesktopWindowContentBridge\", StringComparison.Ordinal)) { bridge = child; }",
                "            }",
                "            return true;",
                "        }, IntPtr.Zero);",
                "        if (inputSite != IntPtr.Zero) { return inputSite; }",
                "        if (bridge != IntPtr.Zero) { return bridge; }",
                "        var point = new POINT { x = screenX, y = screenY };",
                "        if (ScreenToClient(topWindow, ref point)) {",
                "            var child = ChildWindowFromPointEx(topWindow, point, 0);",
                "            if (child != IntPtr.Zero && child != topWindow) { return child; }",
                "        }",
                "        return topWindow;",
                "    }",
                "}",
                "'@",
                $"$targetWindow = [IntPtr]::new({targetWindowValue})",
                "$foreground = $true",
                "if ($targetWindow -ne [IntPtr]::Zero) {",
                "    $foreground = [Native]::SetForegroundWindow($targetWindow)",
                "    Start-Sleep -Milliseconds 200",
                "}",
                $"$ok = [Native]::SetCursorPos({targetX}, {targetY})",
                "Start-Sleep -Milliseconds 100",
                $"$modifier = {modifierValue}",
                "$keyDownSent = 0",
                "if ($modifier -ge 0) {",
                "    [Native]::keybd_event([byte]$modifier, 0, 0, [UIntPtr]::Zero)",
                "    $keyDownSent = 1",
                "    Start-Sleep -Milliseconds 100",
                "}",
                "$client = $true",
                "$messageWindow = $targetWindow",
                "$messageClass = ''",
                "if ($targetWindow -ne [IntPtr]::Zero) {",
                "    $messageWindow = [Native]::ResolveInputWindow($targetWindow, " + targetX.ToString(CultureInfo.InvariantCulture) + ", " + targetY.ToString(CultureInfo.InvariantCulture) + ")",
                "    $messageClass = [Native]::GetClassNameString($messageWindow)",
                "    $point = New-Object Native+POINT",
                $"    $point.x = {targetX}",
                $"    $point.y = {targetY}",
                "    $client = [Native]::ScreenToClient($messageWindow, [ref]$point)",
                "    $mkModifier = if ($modifier -eq 16) { 4 } elseif ($modifier -eq 17) { 8 } else { 0 }",
                "    $lParam = [IntPtr](($point.x -band 0xFFFF) -bor (($point.y -band 0xFFFF) -shl 16))",
                "    [void][Native]::SendMessage($messageWindow, 0x0200, [IntPtr]$mkModifier, $lParam)",
                "    [void][Native]::SendMessage($messageWindow, 0x0201, [IntPtr](1 -bor $mkModifier), $lParam)",
                "    Start-Sleep -Milliseconds 50",
                "    [void][Native]::SendMessage($messageWindow, 0x0202, [IntPtr]$mkModifier, $lParam)",
                "} else {",
                $"    [Native]::mouse_event({MouseEventLeftDown}, 0, 0, 0, [UIntPtr]::Zero)",
                "    Start-Sleep -Milliseconds 50",
                $"    [Native]::mouse_event({MouseEventLeftUp}, 0, 0, 0, [UIntPtr]::Zero)",
                "}",
                "$mouseSent = 2",
                "Start-Sleep -Milliseconds 100",
                "$keyUpSent = 0",
                "if ($modifier -ge 0) {",
                $"    [Native]::keybd_event([byte]$modifier, 0, {KeyEventKeyUp}, [UIntPtr]::Zero)",
                "    $keyUpSent = 1",
                "}",
                $"Set-Content -Path '{escapedResultPath}' -Value (\"ok={{0}};foreground={{1}};client={{2}};window=0x{{3:x}};class={{4}};keydown={{5}};mouse={{6}};keyup={{7}}\" -f $ok, $foreground, $client, $messageWindow.ToInt64(), $messageClass, $keyDownSent, $mouseSent, $keyUpSent)",
                "} catch {",
                $"    Set-Content -Path '{escapedResultPath}' -Value (\"error={{0}}\" -f $_.Exception.Message)",
                "    exit 1",
                "}",
            });

            var launcherContents = string.Join(Environment.NewLine, new[]
            {
                "@echo off",
                $"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"{scriptPath}\"",
            });

            File.WriteAllText(scriptPath, scriptContents);
            File.WriteAllText(launcherPath, launcherContents);
            _scheduledTaskNames.Add(taskName);
            _temporaryLaunchArtifacts.Add(scriptPath);
            _temporaryLaunchArtifacts.Add(launcherPath);
            _temporaryLaunchArtifacts.Add(resultPath);

            _RunProcessAndVerify("schtasks.exe",
                                 $"/Create /TN \"{taskName}\" /SC ONCE /ST 00:00 /RL LIMITED /IT /TR \"\\\"{launcherPath}\\\"\" /F",
                                 $"Create limited input task '{taskName}'.");
            _RunProcessAndVerify("schtasks.exe",
                                 $"/Run /TN \"{taskName}\"",
                                 $"Start limited input task '{taskName}'.");

            var remaining = 10000;
            while (!File.Exists(resultPath) && remaining > 0)
            {
                Thread.Sleep(100);
                remaining -= 100;
            }

            Verify.IsTrue(File.Exists(resultPath), $"Wait for limited input result for {description}.");
            var result = File.ReadAllText(resultPath).Trim();
            Log.Comment($"{description} limited helper result: {result}");
            Verify.IsTrue(!result.StartsWith("error=", StringComparison.OrdinalIgnoreCase), $"Limited helper for {description} should succeed.");
            Verify.IsTrue(result.IndexOf("mouse=2", StringComparison.OrdinalIgnoreCase) >= 0, $"Limited helper should send mouse inputs for {description}.");
            if (modifierKey.HasValue)
            {
                Verify.IsTrue(result.IndexOf("keydown=1", StringComparison.OrdinalIgnoreCase) >= 0 &&
                              result.IndexOf("keyup=1", StringComparison.OrdinalIgnoreCase) >= 0,
                              $"Limited helper should send modifier inputs for {description}.");
            }
            Thread.Sleep(200);
        }

        private static INPUT _CreateKeyboardInput(byte virtualKey, bool keyUp)
        {
            return new INPUT
            {
                type = InputKeyboard,
                U = new INPUTUNION
                {
                    ki = new KEYBDINPUT
                    {
                        wVk = virtualKey,
                        dwFlags = keyUp ? KeyEventKeyUp : 0,
                    },
                },
            };
        }

        private static (int X, int Y) _GetScreenCenter(AppiumWebElement element)
        {
            return
            (
                element.Location.X + (element.Size.Width / 2),
                element.Location.Y + (element.Size.Height / 2)
            );
        }

        private static IntPtr _TryGetWindowHandle(AppiumWebElement element)
        {
            var handle = _TryGetAttribute(element, "NativeWindowHandle");
            if (string.IsNullOrEmpty(handle))
            {
                return IntPtr.Zero;
            }

            if (long.TryParse(handle, NumberStyles.Integer, CultureInfo.InvariantCulture, out var decimalHandle))
            {
                return new IntPtr(decimalHandle);
            }

            handle = handle.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? handle.Substring(2) : handle;
            if (long.TryParse(handle, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var hexHandle))
            {
                return new IntPtr(hexHandle);
            }

            return IntPtr.Zero;
        }

        private void _MoveCursorSmoothly(AppiumWebElement source, int startOffsetX, int startOffsetY, int endOffsetX, int endOffsetY, int steps = 24, int delayMs = 15)
        {
            for (var step = 1; step <= steps; step++)
            {
                var nextX = startOffsetX + ((endOffsetX - startOffsetX) * step / steps);
                var nextY = startOffsetY + ((endOffsetY - startOffsetY) * step / steps);
                _MoveMouseToElementOffset(source, nextX, nextY);
                Thread.Sleep(delayMs);
            }
        }

        private void _MoveMouseToElementOffset(AppiumWebElement element, int offsetX, int offsetY)
        {
            Session.Mouse.MouseMove(element.Coordinates, element.Size.Width / 2 + offsetX, element.Size.Height / 2 + offsetY);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!this.isDisposed)
            {
                // ensure we're exited when this is destroyed or disposed of explicitly
                this.ExitProcess();

                this.isDisposed = true;
            }
        }

        private void CreateProcess(string path, string shellToLaunch, string launchArgs)
        {
            Log.Comment("Attempting to launch command-line application at '{0}'", path);

            string binaryToRunPath = path;
            string args = launchArgs ?? $"new-tab --title \"{_windowTitleToFind}\" --suppressApplicationTitle \"{shellToLaunch}\"";
            if (IsRunningAsAdmin())
            {
                Log.Comment("UIA runner is elevated; launching Terminal at medium integrity through a limited scheduled task.");
                _LaunchLimitedProcess(binaryToRunPath, args);
            }
            else
            {
                job = WinBase.CreateJobObject(IntPtr.Zero, IntPtr.Zero);
                NativeMethods.Win32NullHelper(job, "Creating job object to hold binaries under test.");

                string processCommandLine = $"{binaryToRunPath} {args}";

                WinBase.STARTUPINFO si = new WinBase.STARTUPINFO();
                si.cb = Marshal.SizeOf(si);

                WinBase.PROCESS_INFORMATION pi = new WinBase.PROCESS_INFORMATION();

                NativeMethods.Win32BoolHelper(WinBase.CreateProcess(null,
                    processCommandLine,
                    IntPtr.Zero,
                    IntPtr.Zero,
                    false,
                    WinBase.CP_CreationFlags.CREATE_SUSPENDED,
                    IntPtr.Zero,
                    null,
                    ref si,
                    out pi),
                    "Attempting to create child host window process.");

                Log.Comment($"Host window PID: {pi.dwProcessId}");

                NativeMethods.Win32BoolHelper(WinBase.AssignProcessToJobObject(job, pi.hProcess), "Assigning new host window (suspended) to job object.");
                NativeMethods.Win32BoolHelper(-1 != WinBase.ResumeThread(pi.hThread), "Resume host window process now that it is attached and its launch of the child application will be caught in the job object.");
                _trackedProcessIds.Add(pi.dwProcessId);
            }

            Globals.WaitForTimeout();

            DesiredCapabilities appCapabilities = new DesiredCapabilities();
            appCapabilities.SetCapability("app", @"Root");
            _desktopSession = new IOSDriver<IOSElement>(new Uri(AppDriverUrl), appCapabilities);
            Session = _desktopSession;

            Verify.IsNotNull(_desktopSession);
            Actions = new Actions(Session);
            Verify.IsNotNull(Session);

            var remaining = 30000;
            while (UIRoot == null && remaining >= 0)
            {
                var initialWindow = FindTopLevelWindowByName(_windowTitleToFind);
                if (initialWindow != null)
                {
                    ActivateWindow(initialWindow);
                    User32.GetWindowThreadProcessId(initialWindow.Handle, out var windowPid);
                    if (windowPid != 0)
                    {
                        _trackedProcessIds.Add(unchecked((int)windowPid));
                    }

                    UIRoot = initialWindow.Root;
                }

                if (UIRoot == null)
                {
                    Thread.Sleep(250);
                    remaining -= 250;
                }
            }

            if (UIRoot == null)
            {
                var matchingByName = Session.FindElementsByName(_windowTitleToFind)
                                            .OfType<AppiumWebElement>()
                                            .Where(_IsValidElement)
                                            .Take(30)
                                            .ToList();
                Log.Comment($"Elements visible to WinAppDriver with Name='{_windowTitleToFind}': {matchingByName.Count}");
                foreach (var element in matchingByName)
                {
                    Log.Comment($"Element candidate: Name='{element.GetAttribute("Name")}', ClassName='{element.GetAttribute("ClassName")}', ProcessId='{element.GetAttribute("ProcessId")}'");
                }

                UIRoot = Session.FindElementByXPath("//*");
                Log.Comment("Falling back to desktop root element for UI automation session.");
            }

            Verify.IsNotNull(UIRoot, $"Failed to find a top-level window for '{_windowTitleToFind}'.");

            // Set the timeout to 15 seconds after we found the initial window.
            Session.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(15);
        }

        private void _LaunchLimitedProcess(string path, string args)
        {
            var taskName = $"WindowsTerminal-UIA-{Process.GetCurrentProcess().Id}-{Guid.NewGuid():N}";
            var launcherPath = Path.Combine(Path.GetTempPath(), $"{taskName}.cmd");
            _dragEventLogPath = Path.Combine(Path.GetTempPath(), $"{taskName}.drag.log");
            var launcherContents =
                "@echo off" + Environment.NewLine +
                $"echo launcher-started>\"{_dragEventLogPath}\"" + Environment.NewLine +
                $"set \"WT_UIA_DRAG_LOG={_dragEventLogPath}\"" + Environment.NewLine +
                "set \"WT_UIA_ENABLE_TEST_HOOKS=1\"" + Environment.NewLine +
                $"cd /d \"{Path.GetDirectoryName(path)}\"" + Environment.NewLine +
                $"\"{path}\" {args}" + Environment.NewLine;

            File.WriteAllText(launcherPath, launcherContents);
            _scheduledTaskNames.Add(taskName);
            _temporaryLaunchArtifacts.Add(launcherPath);
            Log.Comment($"Drag event log will be written to '{_dragEventLogPath}'.");

            _RunProcessAndVerify("schtasks.exe",
                                 $"/Create /TN \"{taskName}\" /SC ONCE /ST 00:00 /RL LIMITED /IT /TR \"\\\"{launcherPath}\\\"\" /F",
                                 $"Create limited scheduled task '{taskName}'.");
            _RunProcessAndVerify("schtasks.exe",
                                 $"/Run /TN \"{taskName}\"",
                                 $"Start limited scheduled task '{taskName}'.");
        }

        private static void _RunProcessAndVerify(string fileName, string arguments, string description)
        {
            using (var process = new Process())
            {
                process.StartInfo = new ProcessStartInfo
                {
                    FileName = fileName,
                    Arguments = arguments,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };

                Verify.IsTrue(process.Start(), description);
                var standardOutput = process.StandardOutput.ReadToEnd();
                var standardError = process.StandardError.ReadToEnd();
                process.WaitForExit();

                if (!string.IsNullOrWhiteSpace(standardOutput))
                {
                    Log.Comment(standardOutput.Trim());
                }

                if (!string.IsNullOrWhiteSpace(standardError))
                {
                    Log.Comment(standardError.Trim());
                }

                Verify.AreEqual(0, process.ExitCode, description);
            }
        }

        private static void _TryRunProcess(string fileName, string arguments, string description)
        {
            try
            {
                _RunProcessAndVerify(fileName, arguments, description);
            }
            catch (Exception ex)
            {
                Log.Comment($"{description} failed during cleanup: {ex.Message}");
            }
        }

        private bool IsRunningAsAdmin()
        {
            return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);
        }

        private void ExitProcess()
        {
            Globals.SweepAllModules(this.context);

            // Release attachment to the child process console.
            WinCon.FreeConsole();

            this.UIRoot = null;
            this.Session = null;

            foreach (var session in _windowSessions.Values)
            {
                try
                {
                    session.Quit();
                }
                catch
                {
                }
            }
            _windowSessions.Clear();

            foreach (var taskName in _scheduledTaskNames)
            {
                _TryRunProcess("schtasks.exe",
                               $"/Delete /TN \"{taskName}\" /F",
                               $"Delete limited scheduled task '{taskName}'.");
            }

            foreach (var artifact in _temporaryLaunchArtifacts)
            {
                try
                {
                    File.Delete(artifact);
                }
                catch (Exception ex)
                {
                    Log.Comment($"Delete temporary launch artifact '{artifact}' failed during cleanup: {ex.Message}");
                }
            }

            _scheduledTaskNames.Clear();
            _temporaryLaunchArtifacts.Clear();

            if (_desktopSession != null)
            {
                try
                {
                    _desktopSession.Quit();
                }
                catch
                {
                }
                _desktopSession = null;
            }

            if (this.job != IntPtr.Zero)
            {
                WinBase.TerminateJobObject(this.job, 0);
                this.job = IntPtr.Zero;
            }
            else
            {
                foreach (var pid in _trackedProcessIds.ToArray())
                {
                    try
                    {
                        Process.GetProcessById(pid).Kill();
                    }
                    catch
                    {
                    }
                }

                _trackedProcessIds.Clear();
            }
        }
    }
}
