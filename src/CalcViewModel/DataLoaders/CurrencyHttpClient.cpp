// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// Persistent Calculator currency data is provided by https://frankfurter.dev/.

#include "pch.h"
#include "CurrencyHttpClient.h"

using namespace CalculatorApp::ViewModel::DataLoaders;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;

namespace
{
    constexpr unsigned int MAXIMUM_CURRENCY_RESPONSE_CHARACTERS = 1024 * 1024;
    constexpr auto FRANKFURTER_CURRENCIES_URI = L"https://api.frankfurter.dev/v2/currencies";
    constexpr auto FRANKFURTER_RATES_URI_PREFIX = L"https://api.frankfurter.dev/v2/rates?base=";

    task<String ^> GetJsonAsync(String ^ uriText)
    {
        auto client = ref new HttpClient();
        auto response = co_await client->GetAsync(ref new Uri(uriText));
        if (response == nullptr || !response->IsSuccessStatusCode || response->Content == nullptr)
        {
            co_return nullptr;
        }

        String ^ json = co_await response->Content->ReadAsStringAsync();
        if (json == nullptr || json->IsEmpty() || json->Length() > MAXIMUM_CURRENCY_RESPONSE_CHARACTERS)
        {
            co_return nullptr;
        }

        co_return json;
    }

    bool IsSafeCurrencyCode(String ^ value)
    {
        if (value == nullptr || value->Length() != 3)
        {
            return false;
        }

        for (const wchar_t* current = value->Data(); *current != L'\0'; ++current)
        {
            if (*current < L'A' || *current > L'Z')
            {
                return false;
            }
        }
        return true;
    }
}

void CurrencyHttpClient::Initialize(String ^ sourceCurrencyCode, String ^ responseLanguage)
{
    m_sourceCurrencyCode = sourceCurrencyCode;
    m_responseLanguage = responseLanguage;
}

task<String ^> CurrencyHttpClient::GetCurrencyMetadataAsync() const
{
    (void)m_responseLanguage;
    co_return co_await GetJsonAsync(ref new String(FRANKFURTER_CURRENCIES_URI));
}

task<String ^> CurrencyHttpClient::GetCurrencyRatiosAsync() const
{
    if (!IsSafeCurrencyCode(m_sourceCurrencyCode))
    {
        co_return nullptr;
    }

    std::wstring uri{ FRANKFURTER_RATES_URI_PREFIX };
    uri.append(m_sourceCurrencyCode->Data());
    co_return co_await GetJsonAsync(ref new String(uri.c_str()));
}
