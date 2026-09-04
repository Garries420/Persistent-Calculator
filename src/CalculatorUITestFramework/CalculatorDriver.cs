// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using Microsoft.VisualStudio.TestTools.UnitTesting;

using OpenQA.Selenium.Appium;
using OpenQA.Selenium.Appium.Windows;

using System;
using System.Threading;

namespace CalculatorUITestFramework
{
    public sealed class CalculatorDriver
    {
        private const string defaultAppId = "Garries420.PersistentCalculator_z54wkvydpvvw6!App";

        private static CalculatorDriver instance = null;
        public static CalculatorDriver Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new CalculatorDriver();
                }
                return instance;
            }

        }

        private WinAppDriverLocalServer server;

        public WindowsDriver<WindowsElement> CalculatorSession { get; private set; }

        private CalculatorDriver()
        {
        }

        public void SetupCalculatorSession(TestContext context)
        {
            server = new WinAppDriverLocalServer();

            // Launch Calculator application if it is not yet launched
            if (CalculatorSession == null)
            {
                // Create a new  WinAppDriver session to bring up an instance of the Calculator application
                // Note: Multiple calculator windows (instances) share the same process Id
                var options = new AppiumOptions();

                if (context.Properties.ContainsKey("AppId"))
                {
                    options.AddAdditionalCapability("app", (string)context.Properties["AppId"]);
                }
                else
                {
                    options.AddAdditionalCapability("app", defaultAppId);
                }

                options.AddAdditionalCapability("deviceName", "WindowsPC");
                options.AddAdditionalCapability("ms:waitForAppLaunch", "10");

                // A Persistent Calculator session can take a little longer to exit than
                // the upstream app while its Documents history file is being flushed.
                // Retry a launch that races that suspension rather than failing every
                // remaining test class after one transient WinAppDriver attach error.
                const int maximumLaunchAttempts = 3;
                for (var attempt = 1; attempt <= maximumLaunchAttempts; ++attempt)
                {
                    try
                    {
                        CalculatorSession = new WindowsDriver<WindowsElement>(server.ServiceUrl, options);
                        break;
                    }
                    catch (OpenQA.Selenium.WebDriverException) when (attempt < maximumLaunchAttempts)
                    {
                        Thread.Sleep(TimeSpan.FromSeconds(3));
                    }
                }

                CalculatorSession.Manage().Timeouts().ImplicitWait = TimeSpan.FromSeconds(10);
                Assert.IsNotNull(CalculatorSession);
            }
        }

        public void TearDownCalculatorSession()
        {
            // Close the application and delete the session
            if (CalculatorSession != null)
            {
                CalculatorSession.Quit();
                CalculatorSession = null;

                // Quit initiates UWP suspension asynchronously. Give its deferral time
                // to finish before the next test class launches the same package.
                Thread.Sleep(TimeSpan.FromSeconds(5));
            }

            if (server != null)
            {
                server.Dispose();
                server = null;
            }
        }
    }
}
