// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using OpenQA.Selenium;
using OpenQA.Selenium.Appium.Windows;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Threading;

namespace CalculatorUITestFramework
{
    public class HistoryPanel
    {
        public WindowsElement HistoryButton
        {
            get
            {
                // Older development packages exposed both our global button and
                // Microsoft's collapsed legacy event owner under HistoryButton.
                // Select the visible control so this regression can diagnose
                // those packages as well as the now-unique current source.
                var visibleButton = session
                    .FindElementsByAccessibilityId("HistoryButton")
                    .FirstOrDefault(button => button.Displayed);
                return visibleButton ?? session.FindElementByName("Open saved history");
            }
        }
        public WindowsElement ListViewItem => session.FindElementByClassName("ListViewItem");
        public WindowsElement ClearHistoryButton => session.TryFindElementByAccessibilityId("ClearHistory");

        private WindowsDriver<WindowsElement> session => CalculatorDriver.Instance.CalculatorSession;
        private WindowsElement HistoryListView => session.TryFindElementByAccessibilityId("HistoryListView");
        private WindowsElement HistoryBackButton => session
            .FindElementsByAccessibilityId("HistoryBackButton")
            .FirstOrDefault();

        /// <summary>
        /// Opens the History Pane by clicking the History pivot label.
        /// </summary>
        public void OpenHistoryPanel()
        {
            if (!IsHistoryPageOpen())
            {
                HistoryButton.Click();
                WaitForHistoryPageState(shouldBeOpen: true);
            }
        }

        public void CloseHistoryPanel()
        {
            if (IsHistoryPageOpen())
            {
                HistoryBackButton.Click();
                WaitForHistoryPageState(shouldBeOpen: false);
            }
        }

        private bool IsHistoryPageOpen()
        {
            var backButton = HistoryBackButton;
            return backButton != null && backButton.Displayed;
        }

        private void WaitForHistoryPageState(bool shouldBeOpen)
        {
            const int maxAttempts = 50;
            for (int attempt = 0; attempt < maxAttempts; attempt++)
            {
                if (IsHistoryPageOpen() == shouldBeOpen)
                {
                    return;
                }

                Thread.Sleep(100);
            }

            throw new WebDriverException(
                $"Timed out waiting for the history page to {(shouldBeOpen ? "open" : "close")}.");
        }

        /// <summary>
        /// Gets all of the history items listed in the History Pane.
        /// </summary>
        /// <returns>A readonly collection of history items.</returns>
        public List<HistoryItem> GetAllHistoryListViewItems()
        {
            OpenHistoryPanel();
            return (from item in HistoryListView.FindElementsByClassName("ListViewItem") select new HistoryItem(item)).ToList();
        }

        /// <summary>
        /// Opens the History Pane and clicks the delete button if it is visible.
        /// </summary>
        public void ClearHistory()
        {

            OpenHistoryPanel();
            string source = session.PageSource;
            if (source.Contains("ClearHistory"))
            {
                ClearHistoryButton.Click();
            }
        }

        /// <summary>
        /// If the History label is not displayed, resize the window
        /// Two attempts are made; the label is not found a "not found" exception is thrown
        /// </summary>
        public void ResizeWindowToDisplayHistoryLabel()
        {
            ResizeWindowToDisplayHistoryButton();
        }

        ///// <summary>
        ///// If the History button is not displayed, resize the window
        ///// </summary>
        public void ResizeWindowToDisplayHistoryButton()
        {
            // Put the calculator in the upper left region of the screen
            CalculatorDriver.Instance.CalculatorSession.Manage().Window.Position = new Point(8, 8);
            ShrinkWindowToShowHistoryButton(CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size.Width);
        }

        /// <summary>
        /// Opens the History Flyout.
        /// </summary>
        public void OpenHistoryFlyout()
        {
            OpenHistoryPanel();
        }

        /// <summary>
        /// Gets all of the History items listed in the History Flyout.
        /// </summary>
        /// <returns> A read only collection of History items.</returns>
        public List<HistoryItem> GetAllHistoryFlyoutListViewItems()
        {
            OpenHistoryFlyout();
            return (from item in HistoryListView.FindElementsByClassName("ListViewItem") select new HistoryItem(item)).ToList();
        }

        /// <summary>
        /// Decreases the size of the window until History button is visible
        /// </summary>
        private void ShrinkWindowToShowHistoryButton(int width)
        {
            if (width < 200)
            {
                throw new NotFoundException("Could not find the History Button");
            }

            if (!session.PageSource.Contains("HistoryButton"))
            {
                var height = CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size.Height;
                CalculatorDriver.Instance.CalculatorSession.Manage().Window.Size = new Size(width, height);
                //give window time to render new size
                System.Threading.Thread.Sleep(10);
                ShrinkWindowToShowHistoryButton(width - 100);
            }
        }
    }
}
