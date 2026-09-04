// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "DataLoaders/CurrencyHttpClient.h"

namespace
{
    constexpr auto MockCurrencyConverterData =
        LR"([{"date":"2026-08-08","base":"USD","quote":"EUR","rate":0.920503}])";
    constexpr auto MockCurrencyStaticData =
        LR"([{"iso_code":"USD","name":"United States Dollar","symbol":"$"},{"iso_code":"EUR","name":"Euro","symbol":"\u20ac"}])";
}

namespace CalculatorApp::ViewModel::DataLoaders
{
    bool CurrencyHttpClient::ForceWebFailure = false;
    void CurrencyHttpClient::Initialize(Platform::String ^ sourceCurrencyCode, Platform::String ^ responseLanguage)
    {
        m_sourceCurrencyCode = sourceCurrencyCode;
        m_responseLanguage = responseLanguage;
    }

    concurrency::task<Platform::String ^> CurrencyHttpClient::GetCurrencyMetadataAsync() const
    {
        if (ForceWebFailure)
        {
            throw ref new Platform::Exception(E_FAIL, L"Mocked Network Failure: failed to load currency metadata");
        }
        (void)m_responseLanguage;
        return concurrency::task_from_result<Platform::String ^>(ref new Platform::String(MockCurrencyStaticData));
    }

    concurrency::task<Platform::String ^> CurrencyHttpClient::GetCurrencyRatiosAsync() const
    {
        if (ForceWebFailure)
        {
            throw ref new Platform::Exception(E_FAIL, L"Mocked Network Failure: failed to load currency rates");
        }
        (void)m_sourceCurrencyCode;
        return concurrency::task_from_result<Platform::String ^>(ref new Platform::String(MockCurrencyConverterData));
    }
} // namespace CalculatorApp::ViewModel::DataLoaders
