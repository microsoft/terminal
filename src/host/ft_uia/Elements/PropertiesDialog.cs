//----------------------------------------------------------------------------------------------------------------------
// <copyright file="PropertiesDialog.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating and interacting with the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;

    using OpenQA.Selenium.Appium;

    using Conhost.UIA.Tests.Common;

    public class PropertiesDialog : IDisposable
    {
        public enum CloseAction
        {
            OK,
            Cancel
        }

        public AppiumWebElement PropWindow { get; private set; }
        public AppiumWebElement Tabs { get; private set; }

        private AppiumWebElement okButton;
        private AppiumWebElement cancelButton;

        private CmdApp app;

        private bool isOpened;

        public PropertiesDialog(CmdApp app)
        {
            this.app = app;
        }

        ~PropertiesDialog()
        {
            this.Dispose(false);
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                // if we're being disposed of, close the dialog.
                if (this.isOpened)
                {
                    this.ClosePropertiesDialog(CloseAction.Cancel);
                }
            }
        }

        public void Open(OpenTarget target)
        {
            if (this.isOpened)
            {
                throw new InvalidOperationException("Can't open an already opened window.");
            }

            this.OpenPropertiesDialog(this.app, target);
            this.isOpened = true;
        }

        public void Close(CloseAction action)
        {
            if (!this.isOpened)
            {
                throw new InvalidOperationException("Can't close an unopened window.");
            }

            this.ClosePropertiesDialog(action);
            this.isOpened = false;
        }

        private void OpenPropertiesDialog(CmdApp app, OpenTarget target)
        {
            var titleBar = app.GetTitleBar();
            app.Session.Mouse.ContextClick(titleBar.Coordinates);

            Globals.WaitForTimeout();
            var contextMenu = app.Session.FindElementByClassName(Globals.PopupMenuClassId);

            AppiumWebElement propButton;
            switch (target)
            {
                case OpenTarget.Specifics:
                    propButton = contextMenu.FindElementByName("Properties");
                    break;
                case OpenTarget.Defaults:
                    propButton = contextMenu.FindElementByName("Defaults");
                    break;
                default:
                    throw new NotImplementedException(AutoHelpers.FormatInvariant("Open Properties dialog doesn't yet support target type of '{0}'", target.ToString()));
            }

            propButton.Click();

            Globals.WaitForTimeout();

            this.PropWindow = this.app.UIRoot.FindElementByClassName(Globals.DialogWindowClassId);
            this.Tabs = this.PropWindow.FindElementByClassName("SysTabControl32");

            okButton = this.PropWindow.FindElementByName("OK");
            cancelButton = this.PropWindow.FindElementByName("Cancel");
        }

        private void ClosePropertiesDialog(CloseAction action)
        {
            switch (action)
            {
                case CloseAction.OK:
                    okButton.Click();
                    break;
                case CloseAction.Cancel:
                    cancelButton.Click();
                    break;
            }

            Globals.WaitForTimeout();
        }
    }
}
