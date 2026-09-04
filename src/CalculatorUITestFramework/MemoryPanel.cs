// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using Microsoft.VisualStudio.TestTools.UnitTesting;

using OpenQA.Selenium;
using OpenQA.Selenium.Appium.Windows;
using OpenQA.Selenium.Interactions;

using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace CalculatorUITestFramework
{
    public class MemoryPanel
    {
        public WindowsElement NumberpadMCButton => session.TryFindElementByAccessibilityId("ClearMemoryButton");
        public WindowsElement NumberpadMRButton => session.TryFindElementByAccessibilityId("MemRecall");
        public WindowsElement NumberpadMPlusButton => session.TryFindElementByAccessibilityId("MemPlus");
        public WindowsElement NumberpadMMinusButton => session.TryFindElementByAccessibilityId("MemMinus");
        public WindowsElement NumberpadMSButton => session.TryFindElementByAccessibilityId("memButton");
        public WindowsElement MemoryFlyoutButton => session.TryFindElementByAccessibilityId("MemoryButton");
        public WindowsElement PanelClearMemoryButton => session.TryFindElementByAccessibilityId("ClearMemory");
        public WindowsElement ListViewItem => session.FindElementByClassName("ListViewItem");

        private WindowsDriver<WindowsElement> session => CalculatorDriver.Instance.CalculatorSession;
        private WindowsElement MemoryPane => session.TryFindElementByAccessibilityId("MemoryPanel");
        private WindowsElement MemoryListView => session.TryFindElementByAccessibilityId("MemoryListView");
        private WindowsElement MemoryPaneEmptyLabel => session.TryFindElementByAccessibilityId("MemoryPaneEmpty");
        private WindowsElement MemoryFlyout => session.TryFindElementByAccessibilityId("MemoryFlyout");

        /// <summary>
        /// Opens the Memory list through the compact flyout. The legacy docked
        /// Memory pivot is intentionally absent from Persistent Calculator.
        /// </summary>
        public void OpenMemoryPanel()
        {
            OpenMemoryFlyout();
        }

        /// <summary>
        /// Gets all of the memory items listed in the Memory Pane.
        /// </summary>
        /// <returns>A read-only collection of memory items.</returns>
        public List<MemoryItem> GetAllMemoryListViewItems()
        {
            OpenMemoryPanel();
            return (from item in MemoryListView.FindElementsByClassName("ListViewItem") select new MemoryItem(item)).ToList();
        }

        /// <summary>
        /// Opens the Memory Panel and clicks the delete button if it is visible
        /// </summary>
        public void ClearMemoryPanel()
        {
            OpenMemoryFlyout();

            try
            {
                if (PanelClearMemoryButton != null)
                {
                    PanelClearMemoryButton.Click();
                }
                else
                {
                    return;
                }
            }
            catch (WebDriverException ex)
            {
                if (ex.Message.Contains("element could not be located"))
                {
                    Assert.IsNotNull(MemoryPaneEmptyLabel);
                    return;
                }
                throw;
            }
        }

        /// <summary>
        /// Keeps older tests on the supported compact Memory-button path.
        /// </summary>
        public void ResizeWindowToDisplayMemoryLabel()
        {
            ResizeWindowToDisplayMemoryButton();
        }

        /// <summary>
        /// If the Memory button is not displayed, resize the window
        /// </summary>
        public void ResizeWindowToDisplayMemoryButton()
        {
            // Put the calculator in the upper left region of the screen
            CalculatorDriver.Instance.CalculatorSession.Manage().Window.Position = new Point(8, 8);
            ShrinkWindowToShowMemoryButton(CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size.Width);
        }

        /// <summary>
        /// Opens the Memory Flyout.
        /// </summary>
        public void OpenMemoryFlyout()
        {
            ResizeWindowToDisplayMemoryButton();
            CalculatorApp.EnsureCalculatorHasFocus();
            Actions moveToMemoryButton = new Actions(CalculatorDriver.Instance.CalculatorSession);
            moveToMemoryButton.MoveToElement(MemoryFlyoutButton);
            moveToMemoryButton.Perform();
            CalculatorApp.Window.SendKeys(Keys.Alt + "m" + Keys.Alt);
            Actions moveToMemoryFlyout = new Actions(CalculatorDriver.Instance.CalculatorSession);
            moveToMemoryFlyout.MoveToElement(MemoryFlyout);
            moveToMemoryFlyout.Perform();
        }

        /// <summary>
        /// Gets all of the memory items listed in the Memory Flyout.
        /// </summary>
        /// <returns> A read only collection of memory items.</returns>
        public List<MemoryItem> GetAllMemoryFlyoutListViewItems()
        {
            OpenMemoryFlyout();
            return (from item in MemoryListView.FindElementsByClassName("ListViewItem") select new MemoryItem(item)).ToList();
        }

        /// <summary>
        /// Decreases the size of the window until Memory button is visible
        /// </summary>
        private void ShrinkWindowToShowMemoryButton(int width)
        {
            if (width < 200)
            {
                throw new NotFoundException("Could not find the Memory Button");
            }

            //Page source contains differnt memory button types, using hotkey info is for this specific memory button
            if (!session.PageSource.Contains("Alt, M"))
            {
                var height = CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size.Height;
                CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size = new Size(width, height);
                //give window time to render new size
                System.Threading.Thread.Sleep(10);
                ShrinkWindowToShowMemoryButton(width - 100);
            }
        }
    }
}
