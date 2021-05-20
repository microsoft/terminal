//----------------------------------------------------------------------------------------------------------------------
// <copyright file="TerminalApp.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Helper and wrapper for generating the base test application and its UI Root node.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace WindowsTerminal.UIA.Tests.Elements
{
    using System;
    using System.IO;

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

    public class TerminalApp : IDisposable
    {
        protected const string AppDriverUrl = "http://127.0.0.1:4723";

        private IntPtr job;

        public IOSDriver<IOSElement> Session { get; private set; }
        public Actions Actions { get; private set; }
        public AppiumWebElement UIRoot { get; private set; }

        private bool isDisposed = false;

        private TestContext context;

        public string ContentPath { get; private set; }
        public string GetFullTestContentPath(string filename)
        {
            return Path.GetFullPath(Path.Combine(ContentPath, filename));
        }

        public TerminalApp(TestContext context, string shellToLaunch = "powershell.exe")
        {
            this.context = context;

            // If running locally, set WTPath to where we can find a loose deployment of Windows Terminal
            // On the build machines, the scripts lay it out at the appx\ subfolder of the test deployment directory
            string path = Path.GetFullPath(Path.Combine(context.TestDeploymentDir, @"appx\WindowsTerminal.exe"));
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

            this.CreateProcess(path, shellToLaunch);
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

        protected virtual void Dispose(bool disposing)
        {
            if (!this.isDisposed)
            {
                // ensure we're exited when this is destroyed or disposed of explicitly
                this.ExitProcess();

                this.isDisposed = true;
            }
        }

        private void CreateProcess(string path, string shellToLaunch)
        {
            string WindowTitleToFind = "WindowsTerminal.UIA.Tests";

            job = WinBase.CreateJobObject(IntPtr.Zero, IntPtr.Zero);
            NativeMethods.Win32NullHelper(job, "Creating job object to hold binaries under test.");

            Log.Comment("Attempting to launch command-line application at '{0}'", path);

            string binaryToRunPath = path;
            string args = $"new-tab --title \"{WindowTitleToFind}\" --suppressApplicationTitle \"{shellToLaunch}\"";

            string launchArgs = $"{binaryToRunPath} {args}";

            WinBase.STARTUPINFO si = new WinBase.STARTUPINFO();
            si.cb = Marshal.SizeOf(si);

            WinBase.PROCESS_INFORMATION pi = new WinBase.PROCESS_INFORMATION();

            NativeMethods.Win32BoolHelper(WinBase.CreateProcess(null,
                launchArgs,
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

            Globals.WaitForTimeout();

            DesiredCapabilities appCapabilities = new DesiredCapabilities();
            appCapabilities.SetCapability("app", @"Root");
            Session = new IOSDriver<IOSElement>(new Uri(AppDriverUrl), appCapabilities);

            Verify.IsNotNull(Session);
            Actions = new Actions(Session);
            Verify.IsNotNull(Session);

            Globals.WaitForLongTimeout();

            UIRoot = Session.FindElementByName(WindowTitleToFind);

            // Set the timeout to 15 seconds after we found the initial window.
            Session.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(15);
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

            if (this.job != IntPtr.Zero)
            {
                WinBase.TerminateJobObject(this.job, 0);
            }
            this.job = IntPtr.Zero;
        }
    }
}
