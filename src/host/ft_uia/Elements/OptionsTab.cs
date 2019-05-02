//----------------------------------------------------------------------------------------------------------------------
// <copyright file="OptionsTab.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating and interacting with the Options tab of the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;
    using System.Linq;

    using Conhost.UIA.Tests.Common;
    using NativeMethods = Conhost.UIA.Tests.Common.NativeMethods;
    using OpenQA.Selenium.Appium;

    public class OptionsTab : TabBase
    {
        public List<CheckBoxMeta> Checkboxes { get; private set; }
        public CheckBoxMeta GlobalV1V2Box { get; private set; }
        public AppiumWebElement MoreInfoLink { get; private set; }

        public OptionsTab(PropertiesDialog propDialog) : base(propDialog, " Options ")
        {
        }

        protected override void PopulateItemsOnNavigate(AppiumWebElement propWindow)
        {
            this.GlobalV1V2Box = new CheckBoxMeta(propWindow, "Use legacy console (requires relaunch)", "ForceV2", false, true, false, NativeMethods.WinConP.PKEY_Console_ForceV2);

            this.Checkboxes = new List<CheckBoxMeta>();
            this.Checkboxes.Add(new CheckBoxMeta(propWindow, "Enable line wrapping selection", "LineSelection", false, false, true, NativeMethods.WinConP.PKEY_Console_LineSelection));
            this.Checkboxes.Add(new CheckBoxMeta(propWindow, "Filter clipboard contents on paste", "FilterOnPaste", false, false, true, NativeMethods.WinConP.PKEY_Console_FilterOnPaste));
            this.Checkboxes.Add(new CheckBoxMeta(propWindow, "Enable Ctrl key shortcuts", "CtrlKeyShortcutsDisabled", true, false, true, NativeMethods.WinConP.PKEY_Console_CtrlKeysDisabled));
            this.Checkboxes.Add(new CheckBoxMeta(propWindow, "Extended text selection keys", "ExtendedEditKey", false, true, false, null));
            this.Checkboxes.Add(new CheckBoxMeta(propWindow, "Use Ctrl+Shift+C/V as Copy/Paste", "InterceptCopyPaste", false, false, true, NativeMethods.WinConP.PKEY_Console_InterceptCopyPaste));

            this.MoreInfoLink = propWindow.FindElementByName("new console features");
        }

        public override IEnumerable<AppiumWebElement> GetObjectsDisabledForV1Console()
        {
            return this.Checkboxes.Select(meta => meta.Box);
        }

        public override IEnumerable<AppiumWebElement> GetObjectsUnaffectedByV1V2Switch()
        {
            return new AppiumWebElement[] { this.GlobalV1V2Box.Box, this.MoreInfoLink };
        }

        public override IEnumerable<CheckBoxMeta> GetCheckboxesForVerification()
        {
            return this.Checkboxes;
        }

        public override IEnumerable<SliderMeta> GetSlidersForVerification()
        {
            return new SliderMeta[0];
        }
    }
}
