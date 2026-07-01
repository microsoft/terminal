//----------------------------------------------------------------------------------------------------------------------
// <copyright file="TerminalApp.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Helper and wrapper for generating the base test application and its UI Root node.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace WindowsTerminal.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Threading;

    using WindowsTerminal.UIA.Tests.Common;
    using WindowsTerminal.UIA.Tests.Common.NativeMethods;

    using OpenQA.Selenium;
    using OpenQA.Selenium.Appium;
    using OpenQA.Selenium.Appium.iOS;
    using OpenQA.Selenium.Interactions;
    using OpenQA.Selenium.Remote;

    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    public class TerminalApp : IDisposable
    {
        protected const string AppDriverUrl = "http://127.0.0.1:4723";

        private IntPtr job;
        private bool isDisposed = false;
        private readonly TestContext context;
        private readonly string _windowTitleToFind;
        private readonly Dictionary<IntPtr, IOSDriver<IOSElement>> _windowSessions = new Dictionary<IntPtr, IOSDriver<IOSElement>>();
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

        public IOSDriver<IOSElement> Session { get; private set; }

        public Actions Actions { get; private set; }

        public AppiumWebElement UIRoot { get; private set; }

        public string ContentPath { get; private set; }

        public string GetFullTestContentPath(string filename)
        {
            return Path.GetFullPath(Path.Combine(ContentPath, filename));
        }

        public TerminalApp(TestContext context, string shellToLaunch = "powershell.exe", string launchArgs = null, string windowTitleToFind = "WindowsTerminal.UIA.Tests")
        {
            this.context = context;
            _windowTitleToFind = windowTitleToFind;

            string path = Path.GetFullPath(Path.Combine(context.TestDeploymentDir, @"terminal-0.0.1.0\WindowsTerminal.exe"));
            if (context.Properties.Contains("WTPath"))
            {
                path = (string)context.Properties["WTPath"];
            }
            Log.Comment($"Windows Terminal will be launched from '{path}'");

            ContentPath = @"content";
            if (context.Properties.Contains("WTTestContent"))
            {
                ContentPath = (string)context.Properties["WTTestContent"];
            }
            Log.Comment($"Test Content will be loaded from '{Path.GetFullPath(ContentPath)}'");

            CreateProcess(path, shellToLaunch, launchArgs);
        }

        ~TerminalApp()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        public AppiumWebElement GetRoot()
        {
            return UIRoot;
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
            Actions = new Actions(Session);
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

        public AppiumWebElement FindTabElementByName(TopLevelWindow window, string name)
        {
            return _FindElementsByName(window, name)
                .Where(element => element.Size.Height <= 80 &&
                                  element.Size.Width < window.Width &&
                                  element.Location.Y < window.Bounds.top + 96)
                .OrderBy(element => element.Location.X)
                .ThenBy(element => element.Location.Y)
                .First();
        }

        public bool HasElementByName(TopLevelWindow window, string name)
        {
            return _FindElementsByName(window, name).Any();
        }

        public void SelectTabRangeForTesting(TopLevelWindow window, int startIndex, int endIndex)
        {
            var result = User32.SendMessage(window.Handle, User32.WindowMessages.CM_UIA_SELECT_TAB_RANGE, startIndex, new IntPtr(endIndex));
            Verify.IsTrue(result != IntPtr.Zero, $"Select tab range {startIndex}-{endIndex} for '{window.Title}'.");
            Thread.Sleep(200);
        }

        public void DragByOffset(AppiumWebElement source, int offsetX, int offsetY)
        {
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

        protected virtual void Dispose(bool disposing)
        {
            if (!isDisposed)
            {
                ExitProcess();
                isDisposed = true;
            }
        }

        private IReadOnlyList<AppiumWebElement> _FindElementsByName(TopLevelWindow window, string name)
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

            return matches.ToList();
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
                var existingRoot = _GetRootElement(existingSession);
                if (existingRoot != null)
                {
                    window.Session = existingSession;
                    window.Root = existingRoot;
                    return;
                }

                try
                {
                    existingSession.Quit();
                }
                catch
                {
                }

                _windowSessions.Remove(window.Handle);
            }

            var capabilities = new DesiredCapabilities();
            capabilities.SetCapability("appTopLevelWindow", window.Handle.ToInt64().ToString("x"));

            try
            {
                var windowSession = new IOSDriver<IOSElement>(new Uri(AppDriverUrl), capabilities);
                Verify.IsNotNull(windowSession, $"Attach WinAppDriver session to top-level window '{window.Title}'.");
                windowSession.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(15);

                window.Session = windowSession;
                window.Root = _GetRootElement(windowSession);
                _windowSessions[window.Handle] = windowSession;
            }
            catch
            {
                window.Session = _desktopSession;
                window.Root = _FindWindowRootFromDesktopSession(window) ?? _GetRootElement(_desktopSession);
            }
        }

        private AppiumWebElement _FindWindowRootFromDesktopSession(TopLevelWindow window)
        {
            if (_desktopSession == null)
            {
                return null;
            }

            return _desktopSession.FindElementsByName(window.Title)
                                  .OfType<AppiumWebElement>()
                                  .Where(_IsValidElement)
                                  .Where(element => _IsElementWithinWindow(element, window))
                                  .OrderByDescending(element => element.Size.Width * element.Size.Height)
                                  .FirstOrDefault();
        }

        private static AppiumWebElement _GetRootElement(IOSDriver<IOSElement> session)
        {
            if (session == null)
            {
                return null;
            }

            try
            {
                return session.FindElementByXPath("/*");
            }
            catch
            {
                try
                {
                    return session.FindElementByXPath("//*");
                }
                catch
                {
                    return null;
                }
            }
        }

        private static (int X, int Y) _GetScreenCenter(AppiumWebElement element)
        {
            return
            (
                element.Location.X + (element.Size.Width / 2),
                element.Location.Y + (element.Size.Height / 2)
            );
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

        private void CreateProcess(string path, string shellToLaunch, string launchArgs)
        {
            Log.Comment("Attempting to launch command-line application at '{0}'", path);

            job = WinBase.CreateJobObject(IntPtr.Zero, IntPtr.Zero);
            NativeMethods.Win32NullHelper(job, "Creating job object to hold binaries under test.");

            string binaryToRunPath = path;
            string args = launchArgs ?? $"new-tab --title \"{_windowTitleToFind}\" --suppressApplicationTitle \"{shellToLaunch}\"";
            string processCommandLine = $"{binaryToRunPath} {args}";

            var priorHookValue = Environment.GetEnvironmentVariable("WT_UIA_ENABLE_TEST_HOOKS");
            try
            {
                Environment.SetEnvironmentVariable("WT_UIA_ENABLE_TEST_HOOKS", "1");

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
            }
            finally
            {
                Environment.SetEnvironmentVariable("WT_UIA_ENABLE_TEST_HOOKS", priorHookValue);
            }

            Globals.WaitForTimeout();

            DesiredCapabilities appCapabilities = new DesiredCapabilities();
            appCapabilities.SetCapability("app", @"Root");
            _desktopSession = new IOSDriver<IOSElement>(new Uri(AppDriverUrl), appCapabilities);
            _desktopSession.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(15);
            Session = _desktopSession;
            Actions = new Actions(Session);

            Verify.IsNotNull(_desktopSession);
            Verify.IsNotNull(Session);

            var initialWindow = WaitForTopLevelWindowByName(_windowTitleToFind, null, 30000);
            ActivateWindow(initialWindow);
            Verify.IsNotNull(UIRoot, $"Failed to find a top-level window for '{_windowTitleToFind}'.");
        }

        private void ExitProcess()
        {
            Globals.SweepAllModules(context);

            WinCon.FreeConsole();

            UIRoot = null;
            Session = null;
            Actions = null;

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

            if (job != IntPtr.Zero)
            {
                WinBase.TerminateJobObject(job, 0);
                job = IntPtr.Zero;
            }
        }
    }
}
