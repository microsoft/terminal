//----------------------------------------------------------------------------------------------------------------------
// <copyright file="Tabs.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating all known tabs of the properties dialog.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;
    using System.Linq;

    using Conhost.UIA.Tests.Common;

    public class Tabs : IDisposable
    {
        protected PropertiesDialog propDialog;

        protected List<TabBase> tabs;

        public Tabs(PropertiesDialog propDialog)
        {
            this.propDialog = propDialog;

            this.InitializeAllTabs();
        }

        private void InitializeAllTabs()
        {
            this.tabs = new List<TabBase>();

            this.tabs.Add(new OptionsTab(this.propDialog));
            this.tabs.Add(new FontTab(this.propDialog));
            this.tabs.Add(new LayoutTab(this.propDialog));
            this.tabs.Add(new ColorsTab(this.propDialog));
            
        }

        private Tabs()
        {

        }

        ~Tabs()
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


        public IEnumerable<TabBase> AllTabs
        {
            get
            {
                return this.tabs;
            }
        }

        public enum GlobalState
        {
            ConsoleV1,
            ConsoleV2
        }

        public void SetGlobalState(GlobalState state)
        {
            AutoHelpers.LogInvariant("Updating global state to '{0}'", state.ToString());
            OptionsTab tabWithGlobal = this.tabs.Single(tab => typeof(OptionsTab) == tab.GetType()) as OptionsTab;
            tabWithGlobal.NavigateToTab();

            switch (state)
            {
                case GlobalState.ConsoleV1:
                    tabWithGlobal.GlobalV1V2Box.Check();
                    break;
                case GlobalState.ConsoleV2:
                    tabWithGlobal.GlobalV1V2Box.Uncheck();
                    break;
                default:
                    throw new NotImplementedException();
            }
        }

        public void SetWrapState(bool isWrapOn)
        {
            AutoHelpers.LogInvariant("Updating wrap state to '{0}'", isWrapOn.ToString());
            LayoutTab tabWithWrap = this.tabs.Single(tab => typeof(LayoutTab) == tab.GetType()) as LayoutTab;
            tabWithWrap.NavigateToTab();

            if (isWrapOn)
            {
                tabWithWrap.WrapTextCheckBox.Check();
            }
            else
            {
                tabWithWrap.WrapTextCheckBox.Uncheck();
            }
        }
    }
}