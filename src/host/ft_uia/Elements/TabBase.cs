//----------------------------------------------------------------------------------------------------------------------
// <copyright file="TabBase.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating various tabs of the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;

    using Conhost.UIA.Tests.Common;
    using OpenQA.Selenium.Appium;

    public abstract class TabBase : IDisposable
    {
        protected string tabName;

        protected PropertiesDialog propDialog;

        private TabBase()
        {

        }

        public TabBase(PropertiesDialog propDialog, string tabName)
        {
            this.propDialog = propDialog;
            this.tabName = tabName;
        }

        ~TabBase()
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
            // Don't need to dispose of anything now, but this helps maintain the pattern used by other controls.
        }

        public void NavigateToTab()
        {
            AutoHelpers.LogInvariant("Navigating to '{0}'", this.tabName);
            var tab = this.propDialog.Tabs.FindElementByName(this.tabName);

            tab.Click();
            Globals.WaitForTimeout();
            
            this.PopulateItemsOnNavigate(this.propDialog.PropWindow);

        }

        protected abstract void PopulateItemsOnNavigate(AppiumWebElement propWindow);

        public abstract IEnumerable<AppiumWebElement> GetObjectsDisabledForV1Console();
        public abstract IEnumerable<AppiumWebElement> GetObjectsUnaffectedByV1V2Switch();

        public abstract IEnumerable<CheckBoxMeta> GetCheckboxesForVerification();

        public abstract IEnumerable<SliderMeta> GetSlidersForVerification();
    }
}