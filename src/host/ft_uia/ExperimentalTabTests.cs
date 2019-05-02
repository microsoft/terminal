//----------------------------------------------------------------------------------------------------------------------
// <copyright file="ExperimentalTabTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for the Experimental Tab control in the Console Properties dialog window.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Threading;

    using Microsoft.Win32;

    using WEX.Common.Managed;
    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using Conhost.UIA.Tests.Elements;
    using OpenQA.Selenium.Appium;

    [TestClass]
    public class ExperimentalTabTests
    {
        public TestContext TestContext { get; set; }

        public const int timeout = Globals.Timeout;

        [TestMethod]
        public void CheckExperimentalDisableState()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry(); // manipulating the global v1/v2 state can affect the registry so back it up.

                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (PropertiesDialog properties = new PropertiesDialog(app))
                    {
                        properties.Open(OpenTarget.Defaults);

                        using (Tabs tabs = new Tabs(properties))
                        {
                            // check everything stays enabled when global is on.
                            AutoHelpers.LogInvariant("Check that items are all enabled when global is enabled.");
                            tabs.SetGlobalState(Tabs.GlobalState.ConsoleV2);

                            // iterate through each tab
                            AutoHelpers.LogInvariant("Checking elements on all tabs.");
                            foreach (TabBase tab in tabs.AllTabs)
                            {
                                tab.NavigateToTab();

                                IEnumerable<AppiumWebElement> itemsUnaffected = tab.GetObjectsUnaffectedByV1V2Switch();
                                IEnumerable<AppiumWebElement> itemsThatDisable = tab.GetObjectsDisabledForV1Console();

                                foreach (AppiumWebElement obj in itemsThatDisable.Concat(itemsUnaffected))
                                {
                                    Verify.IsTrue(obj.Enabled, AutoHelpers.FormatInvariant("Option: {0}", obj.Text));
                                }
                            }

                            // check that relevant boxes are disabled when global is off.
                            AutoHelpers.LogInvariant("Check that necessary items are disabled when global is disabled.");
                            tabs.SetGlobalState(Tabs.GlobalState.ConsoleV1);

                            foreach (TabBase tab in tabs.AllTabs)
                            {
                                tab.NavigateToTab();

                                IEnumerable<AppiumWebElement> itemsUnaffected = tab.GetObjectsUnaffectedByV1V2Switch();
                                IEnumerable<AppiumWebElement> itemsThatDisable = tab.GetObjectsDisabledForV1Console();

                                foreach (AppiumWebElement obj in itemsThatDisable)
                                {
                                    Verify.IsFalse(obj.Enabled, AutoHelpers.FormatInvariant("Option: {0}", obj.Text));
                                }
                                foreach (AppiumWebElement obj in itemsUnaffected)
                                {
                                    Verify.IsTrue(obj.Enabled, AutoHelpers.FormatInvariant("Option: {0}", obj.Text));
                                }
                            }
                        }
                    }
                }
            }
        }

        [TestMethod]
        public void CheckRegistryWritebacks()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();

                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    this.CheckRegistryWritebacks(reg, app, OpenTarget.Defaults);
                    this.CheckRegistryWritebacks(reg, app, OpenTarget.Specifics);
                }
            }
        }

        [TestMethod]
        public void CheckShortcutWritebacks()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                // The global state changes can still impact the registry, so back up the registry anyway despite this being the shortcut test.
                reg.BackupRegistry();

                using (ShortcutHelper shortcut = new ShortcutHelper())
                {
                    shortcut.CreateTempCmdShortcut();

                    using (CmdApp app = new CmdApp(CreateType.ShortcutFile, TestContext, shortcut.ShortcutPath))
                    {
                        this.CheckShortcutWritebacks(shortcut, app, OpenTarget.Specifics);
                    }
                }
            }
        }

        private void CheckRegistryWritebacks(RegistryHelper reg, CmdApp app, OpenTarget target)
        {
            this.CheckWritebacks(reg, null, app, target);
        }

        private void CheckShortcutWritebacks(ShortcutHelper shortcut, CmdApp app, OpenTarget target)
        {
            this.CheckWritebacks(null, shortcut, app, target);
        }

        private void CheckWritebacks(RegistryHelper reg, ShortcutHelper shortcut, CmdApp app, OpenTarget target)
        {
            // either registry or shortcut are null
            if ((reg == null && shortcut == null) || (reg != null && shortcut != null))
            {
                throw new NotSupportedException("Must leave either registry or shortcut null. And must supply one of the two.");
            }

            bool isRegMode = reg != null; // true is reg mode, false is shortcut mode

            string modeName = isRegMode ? "registry" : "shortcut";

            AutoHelpers.LogInvariant("Beginning {0} writeback tests for {1}", modeName, target.ToString());

            using (PropertiesDialog props = new PropertiesDialog(app))
            {
                // STEP 1: VERIFY EVERYTHING SAVES IN AN ON/MAX STATE
                AutoHelpers.LogInvariant("Open dialog and check boxes.");
                props.Open(target);

                using (Tabs tabs = new Tabs(props))
                {
                    // Set V2 on.
                    tabs.SetGlobalState(Tabs.GlobalState.ConsoleV2);

                    AutoHelpers.LogInvariant("Toggling elements on all tabs.");
                    foreach (TabBase tab in tabs.AllTabs)
                    {
                        tab.NavigateToTab();

                        foreach (CheckBoxMeta obj in tab.GetCheckboxesForVerification())
                        {
                            obj.Check();
                        }

                        foreach (SliderMeta obj in tab.GetSlidersForVerification())
                        {
                            // adjust slider to the maximum
                            obj.SetToMaximum();
                        }
                    }

                    AutoHelpers.LogInvariant("Hit OK to save.");
                    props.Close(PropertiesDialog.CloseAction.OK);

                    AutoHelpers.LogInvariant("Verify values changed as appropriate.");
                    CheckWritebacksVerifyValues(isRegMode, reg, shortcut, target, tabs, SliderMeta.ExpectedPosition.Maximum, false, Tabs.GlobalState.ConsoleV2);
                }

                // STEP 2: VERIFY EVERYTHING SAVES IN AN OFF/MIN STATE
                AutoHelpers.LogInvariant("Open dialog and uncheck boxes.");
                props.Open(target);

                using (Tabs tabs = new Tabs(props))
                {
                    AutoHelpers.LogInvariant("Toggling elements on all tabs.");
                    foreach (TabBase tab in tabs.AllTabs)
                    {
                        tab.NavigateToTab();

                        foreach (SliderMeta slider in tab.GetSlidersForVerification())
                        {
                            // adjust slider to the minimum
                            slider.SetToMinimum();
                        }

                        foreach (CheckBoxMeta obj in tab.GetCheckboxesForVerification())
                        {
                            obj.Uncheck();
                        }
                    }

                    tabs.SetGlobalState(Tabs.GlobalState.ConsoleV1);

                    AutoHelpers.LogInvariant("Hit OK to save.");
                    props.Close(PropertiesDialog.CloseAction.OK);

                    AutoHelpers.LogInvariant("Verify values changed as appropriate.");
                    CheckWritebacksVerifyValues(isRegMode, reg, shortcut, target, tabs, SliderMeta.ExpectedPosition.Minimum, true, Tabs.GlobalState.ConsoleV1);
                }

                // STEP 3: VERIFY CANCEL DOES NOT SAVE
                AutoHelpers.LogInvariant("Open dialog and check boxes.");
                props.Open(target);

                using (Tabs tabs = new Tabs(props))
                {
                    tabs.SetGlobalState(Tabs.GlobalState.ConsoleV2);

                    AutoHelpers.LogInvariant("Toggling elements on all tabs.");
                    foreach (TabBase tab in tabs.AllTabs)
                    {
                        tab.NavigateToTab();

                        foreach (CheckBoxMeta obj in tab.GetCheckboxesForVerification())
                        {
                            obj.Check();
                        }

                        foreach (SliderMeta obj in tab.GetSlidersForVerification())
                        {
                            // adjust slider to the maximum
                            obj.SetToMaximum();
                        }
                    }

                    AutoHelpers.LogInvariant("Hit cancel to not save.");
                    props.Close(PropertiesDialog.CloseAction.Cancel);

                    AutoHelpers.LogInvariant("Verify values did not change.");
                    CheckWritebacksVerifyValues(isRegMode, reg, shortcut, target, tabs, SliderMeta.ExpectedPosition.Minimum, true, Tabs.GlobalState.ConsoleV1);
                }
            }
        }

        private void CheckWritebacksVerifyValues(bool isRegMode, RegistryHelper reg, ShortcutHelper shortcut, OpenTarget target, Tabs tabs, SliderMeta.ExpectedPosition sliderExpected, bool checkboxValue, Tabs.GlobalState consoleVersion)
        {
            foreach (TabBase tab in tabs.AllTabs)
            {
                CheckWritebacksVerifyValues(isRegMode, reg, shortcut, target, tab, sliderExpected, checkboxValue, consoleVersion);
            }
        }

        private void CheckWritebacksVerifyValues(bool isRegMode, RegistryHelper reg, ShortcutHelper shortcut, OpenTarget target, TabBase tab, SliderMeta.ExpectedPosition sliderExpected, bool checkboxValue, Tabs.GlobalState consoleVersion)
        {
            if (isRegMode)
            {
                VerifyBoxes(tab, reg, checkboxValue, target, consoleVersion);
                VerifySliders(tab, reg, sliderExpected, target, consoleVersion);
            }
            else
            {
                // Have to wait for shortcut to get written. 
                // There isn't really an event to know when this occurs, so just wait.
                Globals.WaitForTimeout();

                VerifyBoxes(tab, shortcut, checkboxValue, consoleVersion);
                VerifySliders(tab, shortcut, sliderExpected, consoleVersion);
            }
        }

        private void VerifyBoxes(TabBase tab, RegistryHelper reg, bool inverse, OpenTarget target, Tabs.GlobalState consoleVersion)
        {
            // get the key for the current target
            RegistryKey consoleKey = reg.GetMatchingKey(target);

            // hold the parent console key in case we need to look things up for specifics.
            RegistryKey parentConsoleKey = reg.GetMatchingKey(OpenTarget.Defaults);

            // include the global checkbox in the set for verification purposes
            IEnumerable<CheckBoxMeta> boxes = tab.GetCheckboxesForVerification();

            AutoHelpers.LogInvariant("Testing target: {0} in inverse {1} mode", target.ToString(), inverse.ToString());

            // If we're opened as specifics, remove all global only boxes from the test set
            if (target == OpenTarget.Specifics)
            {
                AutoHelpers.LogInvariant("Reducing");
                boxes = boxes.Where(box => !box.IsGlobalOnly);
            }

            foreach (CheckBoxMeta meta in boxes)
            {
                int? storedValue = consoleKey.GetValue(meta.ValueName) as int?;

                string boxName = AutoHelpers.FormatInvariant("Box: {0}", meta.ValueName);

                // if we're in specifics mode, we might have a null and if so, we check the parent value
                if (target == OpenTarget.Specifics)
                {
                    if (storedValue == null)
                    {
                        AutoHelpers.LogInvariant("Specific setting missing. Checking defaults.");
                        storedValue = parentConsoleKey.GetValue(meta.ValueName) as int?;
                    }
                }
                else
                {
                    Verify.IsNotNull(storedValue, boxName);
                }

                if (consoleVersion == Tabs.GlobalState.ConsoleV1 && meta.IsV2Property)
                {
                    AutoHelpers.LogInvariant("Skipping validation of v2 property {0} after switching to v1 console.", meta.ValueName);
                }
                else
                {
                    // A box can be inverse if checking it means false in the registry.
                    // This method can be inverse if we're turning off the boxes and expecting it to be on.
                    // Therefore, a box will be false if it's checked and supposed to be off. Or if it's unchecked and supposed to be on.
                    if ((meta.IsInverse && !inverse) || (!meta.IsInverse && inverse))
                    {
                        Verify.IsFalse(storedValue.Value.DwordToBool(), boxName);
                    }
                    else
                    {
                        Verify.IsTrue(storedValue.Value.DwordToBool(), boxName);
                    }
                }
            }
        }

        private void VerifyBoxes(TabBase tab, ShortcutHelper shortcut, bool inverse, Tabs.GlobalState consoleVersion)
        {
            IEnumerable<CheckBoxMeta> boxes = tab.GetCheckboxesForVerification();

            // collect up properties that we need to retrieve keys for
            IEnumerable<CheckBoxMeta> propBoxes = boxes.Where(box => box.PropKey != null);
            IEnumerable<Wtypes.PROPERTYKEY> keys = propBoxes.Select(box => box.PropKey).Cast<Wtypes.PROPERTYKEY>();

            // fetch data for keys
            IDictionary<Wtypes.PROPERTYKEY, object> propertyData = shortcut.GetFromPropertyStore(keys);

            // enumerate each box and validate the data
            foreach (CheckBoxMeta meta in propBoxes)
            {
                string boxName = AutoHelpers.FormatInvariant("Box: {0}", meta.ValueName);

                Wtypes.PROPERTYKEY key = (Wtypes.PROPERTYKEY)meta.PropKey;

                bool? value = (bool?)propertyData[key];

                Verify.IsNotNull(value, boxName);

                if (consoleVersion == Tabs.GlobalState.ConsoleV1 && meta.IsV2Property)
                {
                    AutoHelpers.LogInvariant("Skipping validation of v2 property {0} after switching to v1 console.", meta.ValueName);
                }
                else
                {
                    // A box can be inverse if checking it means false in the registry.
                    // This method can be inverse if we're turning off the boxes and expecting it to be on.
                    // Therefore, a box will be false if it's checked and supposed to be off. Or if it's unchecked and supposed to be on.
                    if ((meta.IsInverse && !inverse) || (!meta.IsInverse && inverse))
                    {
                        Verify.IsFalse(value.Value, boxName);
                    }
                    else
                    {
                        Verify.IsTrue(value.Value, boxName);
                    }
                }
            }
        }

        private void VerifySliders(TabBase tab, RegistryHelper reg, SliderMeta.ExpectedPosition expected, OpenTarget target, Tabs.GlobalState consoleVersion)
        {
            // get the key for the current target
            RegistryKey consoleKey = reg.GetMatchingKey(target);

            // hold the parent console key in case we need to look things up for specifics.
            RegistryKey parentConsoleKey = reg.GetMatchingKey(OpenTarget.Defaults);

            IEnumerable<SliderMeta> sliders = tab.GetSlidersForVerification();

            foreach (SliderMeta meta in sliders)
            {
                int? storedValue = consoleKey.GetValue(meta.ValueName) as int?;

                string sliderName = AutoHelpers.FormatInvariant("Slider: {0}", meta.ValueName);

                if (target == OpenTarget.Specifics)
                {
                    if (storedValue == null)
                    {
                        AutoHelpers.LogInvariant("Specific setting missing. Checking defaults.");
                        storedValue = parentConsoleKey.GetValue(meta.ValueName) as int?;
                    }
                }
                else
                {
                    Verify.IsNotNull(storedValue, sliderName);
                }

                int transparency = 0;

                switch (expected)
                {
                    case SliderMeta.ExpectedPosition.Maximum:
                        transparency = meta.GetMaximum();
                        break;
                    case SliderMeta.ExpectedPosition.Minimum:
                        transparency = meta.GetMinimum();
                        break;
                    default:
                        throw new NotImplementedException();
                }

                if (consoleVersion == Tabs.GlobalState.ConsoleV1 && meta.IsV2Property)
                {
                    AutoHelpers.LogInvariant("Skipping validation of v2 property {0} after switching to v1 console.", meta.ValueName);
                }
                else
                {
                    Verify.AreEqual(storedValue.Value, RescaleSlider(transparency), sliderName);
                }
            }
        }

        private void VerifySliders(TabBase tab, ShortcutHelper shortcut, SliderMeta.ExpectedPosition expected, Tabs.GlobalState consoleVersion)
        {
            IEnumerable<SliderMeta> sliders = tab.GetSlidersForVerification();

            // collect up properties that we need to retrieve keys for
            IEnumerable<SliderMeta> propSliders = sliders.Where(slider => slider.PropKey != null);
            IEnumerable<Wtypes.PROPERTYKEY> keys = propSliders.Select(slider => slider.PropKey).Cast<Wtypes.PROPERTYKEY>();

            // fetch data for keys
            IDictionary<Wtypes.PROPERTYKEY, object> propertyData = shortcut.GetFromPropertyStore(keys);

            // enumerate each slider and validate data
            foreach (SliderMeta meta in sliders)
            {
                string sliderName = AutoHelpers.FormatInvariant("Slider: {0}", meta.ValueName);

                Wtypes.PROPERTYKEY key = (Wtypes.PROPERTYKEY)meta.PropKey;

                short value = (short)propertyData[key];

                int transparency = 0;

                switch (expected)
                {
                    case SliderMeta.ExpectedPosition.Maximum:
                        transparency = meta.GetMaximum();
                        break;
                    case SliderMeta.ExpectedPosition.Minimum:
                        transparency = meta.GetMinimum();
                        break;
                    default:
                        throw new NotImplementedException();
                }

                if (consoleVersion == Tabs.GlobalState.ConsoleV1 && meta.IsV2Property)
                {
                    AutoHelpers.LogInvariant("Skipping validation of v2 property {0} after switching to v1 console.", meta.ValueName);
                }
                else
                {
                    Verify.AreEqual(value, RescaleSlider(transparency), sliderName);
                }
            }
        }

        private short RescaleSlider(int inputValue)
        {
            // we go on a scale from 0x4D to 0xFF.
            int minValue = 0x4D;
            int maxValue = 0xFF;

            int valueRange = maxValue - minValue;

            double scaleFactor = (double)inputValue / 100.0;

            short finalValue = (short)((valueRange * scaleFactor) + minValue);

            return finalValue;
        }
    }
}
