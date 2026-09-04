// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using CalculatorUITestFramework;

using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CalculatorUITests
{
    [TestClass]
    public class HistoryNavigationSmokeTests
    {
        private static readonly StandardCalculatorPage page = new StandardCalculatorPage();

        [ClassInitialize]
        public static void ClassInitialize(TestContext context)
        {
            CalculatorDriver.Instance.SetupCalculatorSession(context);
            page.HistoryPanel.CloseHistoryPanel();
            page.NavigateToStandardCalculator();
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            CalculatorDriver.Instance.TearDownCalculatorSession();
        }

        [TestMethod]
        [Priority(0)]
        public void GlobalHistoryButton_OpensAndClosesPersistentHistory()
        {
            page.HistoryPanel.OpenHistoryPanel();
            Assert.IsNotNull(
                CalculatorDriver.Instance.CalculatorSession.FindElementByAccessibilityId("HistoryBackButton"));
            page.HistoryPanel.CloseHistoryPanel();
        }
    }
}
