//----------------------------------------------------------------------------------------------------------------------
// <copyright file="ColorsTab.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating and interacting with the Colors tab of the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;

    using Conhost.UIA.Tests.Common;
    using NativeMethods = Conhost.UIA.Tests.Common.NativeMethods;
    using OpenQA.Selenium.Appium;

    public class ColorsTab : TabBase
    {
        public SliderMeta OpacitySlider { get; private set; }

        public ColorsTab(PropertiesDialog propDialog) : base(propDialog, " Colors ")
        {
        }

        protected override void PopulateItemsOnNavigate(AppiumWebElement propWindow)
        {
            var slider = propWindow.FindElementByClassName("msctls_trackbar32");

            this.OpacitySlider = new SliderMeta(slider, "WindowAlpha", true, NativeMethods.WinConP.PKEY_Console_WindowTransparency);
        }

        public override IEnumerable<AppiumWebElement> GetObjectsDisabledForV1Console()
        {
            return new AppiumWebElement[] { this.OpacitySlider.Slider };
        }

        public override IEnumerable<AppiumWebElement> GetObjectsUnaffectedByV1V2Switch()
        {
            return new AppiumWebElement[0];
        }

        public override IEnumerable<CheckBoxMeta> GetCheckboxesForVerification()
        {
            return new CheckBoxMeta[0];
        }

        public override IEnumerable<SliderMeta> GetSlidersForVerification()
        {
            return new SliderMeta[] { this.OpacitySlider };
        }
    }
}