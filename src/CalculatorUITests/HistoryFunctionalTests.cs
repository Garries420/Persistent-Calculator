// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using CalculatorUITestFramework;

using Microsoft.VisualStudio.TestTools.UnitTesting;

using OpenQA.Selenium.Interactions;

using System;
using System.Threading;

namespace CalculatorUITests
{
    [TestClass]
    public class HistoryFunctionalTests
    {
        private static readonly StandardCalculatorPage page = new StandardCalculatorPage();

        private static string NormalizeDecimalSeparator(string value)
        {
            return value?.Replace(',', '.');
        }

        private static string NormalizeIntegerDisplay(string value)
        {
            return value?
                .Replace(" ", string.Empty)
                .Replace("\u00A0", string.Empty)
                .Replace("\u202F", string.Empty);
        }

        [ClassInitialize]
        public static void ClassInitialize(TestContext context)
        {
            CalculatorDriver.Instance.SetupCalculatorSession(context);
            page.HistoryPanel.CloseHistoryPanel();
            page.NavigateToStandardCalculator();
            page.MemoryPanel.ResizeWindowToDisplayMemoryLabel();
        }

        [ClassCleanup]
        public static void ClassCleanup()
        {
            CalculatorDriver.Instance.TearDownCalculatorSession();
        }

        [TestInitialize]
        public void TestInit()
        {
            page.HistoryPanel.CloseHistoryPanel();
            CalculatorApp.EnsureCalculatorHasFocus();
            page.EnsureCalculatorIsInStandardMode();
            page.ClearAll();
            page.HistoryPanel.ClearHistory();
            page.HistoryPanel.CloseHistoryPanel();
        }

        [TestCleanup]
        public void TestCleanup()
        {
            page.HistoryPanel.CloseHistoryPanel();
            page.EnsureCalculatorIsInStandardMode();
            page.ClearAll();
        }

        [TestMethod]
        [Priority(2)]
        public void StandardHistory_CreatesCohesiveSessionsAndRecallsResults()
        {
            page.StandardOperators.NumberPad.Input(-3);
            page.StandardOperators.PlusButton.Click();
            page.StandardOperators.NumberPad.Input(-2.6);
            page.StandardOperators.EqualButton.Click();

            // C closes the first draft and starts a new cohesive session.
            page.StandardOperators.ClearButton.Click();
            page.StandardOperators.NumberPad.Input(2);
            page.StandardOperators.MinusButton.Click();
            page.StandardOperators.NumberPad.Input(3);
            page.StandardOperators.EqualButton.Click();

            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(2, historyItems.Count);
            Assert.AreEqual("-1", historyItems[0].GetValue());
            Assert.IsTrue(historyItems[0].GetExpression().Contains("2"));
            Assert.IsTrue(historyItems[0].GetExpression().Contains("3"));
            Assert.AreEqual("-5.6", NormalizeDecimalSeparator(historyItems[1].GetValue()));
            Assert.IsTrue(historyItems[1].GetExpression().Contains("-3"));

            Actions recallOlderItem = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallOlderItem.Click(historyItems[1].Item);
            recallOlderItem.Perform();

            Assert.AreEqual("-5.6", NormalizeDecimalSeparator(page.CalculatorResults.GetCalculatorResultText()));
            Assert.IsTrue(page.CalculatorResults.GetCalculatorExpressionText().Contains("-3"));
        }

        [TestMethod]
        [Priority(2)]
        public void StandardHistory_KeepsIntermediateResultsInOneEntry()
        {
            page.StandardOperators.NumberPad.Input(4);
            page.StandardOperators.MultiplyButton.Click();
            page.StandardOperators.NumberPad.Input(5);
            page.StandardOperators.DivideButton.Click();
            page.StandardOperators.NumberPad.Input(2);
            page.StandardOperators.EqualButton.Click();

            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(1, historyItems.Count);
            Assert.AreEqual("10", historyItems[0].GetValue());
            Assert.IsTrue(historyItems[0].GetExpression().Contains("4"));
            Assert.IsTrue(historyItems[0].GetExpression().Contains("20"));
            Assert.IsTrue(historyItems[0].GetExpression().EndsWith("=", StringComparison.Ordinal));
        }

        [TestMethod]
        [Priority(2)]
        public void ProgrammerHistory_RestoresOriginalRadixAndValue()
        {
            var programmerPage = new ProgrammerCalculatorPage();
            programmerPage.NavigationMenu.ChangeCalculatorMode(CalculatorMode.ProgrammerCalculator);
            programmerPage.ClearAll();
            programmerPage.ProgrammerOperators.ResetWordSize();
            programmerPage.ProgrammerOperators.ResetNumberSystem();
            programmerPage.StandardOperators.NumberPad.Input(9856);
            Thread.Sleep(300);

            programmerPage.ProgrammerOperators.OctButton.Click();
            Assert.AreEqual("23200", NormalizeIntegerDisplay(programmerPage.CalculatorResults.GetCalculatorResultText()));

            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(1, historyItems.Count);

            Actions recallDecimalEntry = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallDecimalEntry.Click(historyItems[0].Item);
            recallDecimalEntry.Perform();

            Assert.AreEqual("Programmer", programmerPage.Header.Text);
            Assert.AreEqual("9856", NormalizeIntegerDisplay(programmerPage.CalculatorResults.GetCalculatorResultText()));
            Assert.IsTrue(programmerPage.ProgrammerOperators.DecButton.Selected);
        }

        [TestMethod]
        [Priority(2)]
        public void ScientificHistory_RestoresOriginalAngleAndResult()
        {
            var scientificPage = new ScientificCalculatorPage();
            scientificPage.NavigationMenu.ChangeCalculatorMode(CalculatorMode.ScientificCalculator);
            scientificPage.ClearAll();
            scientificPage.ScientificOperators.SetAngleOperator(AngleOperatorState.Degrees);
            scientificPage.StandardOperators.NumberPad.Input(30);
            scientificPage.ScientificOperators.SinButton.Click();
            Thread.Sleep(300);

            string expectedResult = scientificPage.CalculatorResults.GetCalculatorResultText();
            scientificPage.ScientificOperators.SetAngleOperator(AngleOperatorState.Radians);

            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(1, historyItems.Count);

            Actions recallDegreeEntry = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallDegreeEntry.Click(historyItems[0].Item);
            recallDegreeEntry.Perform();

            Assert.AreEqual("Scientific", scientificPage.Header.Text);
            Assert.AreEqual(expectedResult, scientificPage.CalculatorResults.GetCalculatorResultText());
            Assert.AreEqual(
                "degButton",
                scientificPage.ScientificOperators.AngleOperator.GetAttribute("AutomationId"));
        }

        [TestMethod]
        [Priority(2)]
        public void StandardHistory_ClearAllUsesExplicitGlobalAction()
        {
            page.StandardOperators.NumberPad.Input(2);
            page.StandardOperators.PlusButton.Click();
            page.StandardOperators.NumberPad.Input(3);
            page.StandardOperators.EqualButton.Click();

            page.HistoryPanel.OpenHistoryPanel();
            Assert.IsNotNull(page.HistoryPanel.ClearHistoryButton);
            page.HistoryPanel.ClearHistoryButton.Click();
            Assert.IsNotNull(CalculatorDriver.Instance.CalculatorSession.FindElementByAccessibilityId("HistoryEmpty"));
        }

        [TestMethod]
        [Priority(2)]
        public void ConverterHistory_RecallsSourceValueInsteadOfResettingToZero()
        {
            var converterPage = new UnitConverterPage();
            converterPage.NavigationMenu.ChangeCalculatorMode(CalculatorMode.Area);
            converterPage.UnitConverterResults.IsResultsDisplayPresent();
            converterPage.ClearAll();
            converterPage.UnitConverterOperators.NumberPad.Num4Button.Click();
            converterPage.UnitConverterOperators.NumberPad.Num8Button.Click();

            Thread.Sleep(300);
            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.IsTrue(historyItems.Count > 0);
            Assert.IsTrue(historyItems[0].GetExpression().Contains("48"));

            Actions recallNewestItem = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallNewestItem.Click(historyItems[0].Item);
            recallNewestItem.Perform();

            Assert.AreEqual(
                "Area",
                CalculatorDriver.Instance.CalculatorSession.FindElementByAccessibilityId("Header").Text);
            Assert.AreEqual("48", converterPage.UnitConverterResults.GetCalculationResult1Text());
            Assert.AreNotEqual("0", converterPage.UnitConverterResults.GetCalculationResult2Text());
        }

        [TestMethod]
        [Priority(2)]
        public void ConverterHistory_RecallDoesNotCreateDuplicateEntries()
        {
            var converterPage = new UnitConverterPage();
            converterPage.NavigationMenu.ChangeCalculatorMode(CalculatorMode.Volume);
            converterPage.ClearAll();
            converterPage.UnitConverterOperators.NumberPad.Input(56849);
            Thread.Sleep(300);

            converterPage.NavigationMenu.ChangeCalculatorMode(CalculatorMode.Data);
            converterPage.ClearAll();
            converterPage.UnitConverterOperators.NumberPad.Input(78);
            Thread.Sleep(300);

            var historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(2, historyItems.Count);

            Actions recallVolume = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallVolume.Click(historyItems[1].Item);
            recallVolume.Perform();
            Thread.Sleep(300);

            historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(2, historyItems.Count);
            Assert.AreEqual(
                "Volume",
                CalculatorDriver.Instance.CalculatorSession.FindElementByAccessibilityId("Header").Text);
            Assert.AreEqual("56849", converterPage.UnitConverterResults.GetCalculationResult1Text());

            Actions recallData = new Actions(CalculatorDriver.Instance.CalculatorSession);
            recallData.Click(historyItems[0].Item);
            recallData.Perform();
            Thread.Sleep(300);

            historyItems = page.HistoryPanel.GetAllHistoryListViewItems();
            Assert.AreEqual(2, historyItems.Count);
            Assert.AreEqual(
                "Data",
                CalculatorDriver.Instance.CalculatorSession.FindElementByAccessibilityId("Header").Text);
            Assert.AreEqual("78", converterPage.UnitConverterResults.GetCalculationResult1Text());
        }
    }
}
