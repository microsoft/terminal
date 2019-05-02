//----------------------------------------------------------------------------------------------------------------------
// <copyright file="FontTab.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating and interacting with the Font tab of the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;

    using Conhost.UIA.Tests.Common;
    using NativeMethods = Conhost.UIA.Tests.Common.NativeMethods;
    using OpenQA.Selenium.Appium;

    public class FontTab : TabBase
    {
        public FontTab(PropertiesDialog propDialog) : base(propDialog, " Font ")
        {

        }

        protected override void PopulateItemsOnNavigate(AppiumWebElement propWindow)
        {
            return;
        }

        public override IEnumerable<AppiumWebElement> GetObjectsDisabledForV1Console()
        {
            return new AppiumWebElement[0];
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
            return new SliderMeta[0];
        }
    }
}