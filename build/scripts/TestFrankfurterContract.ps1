[CmdletBinding()]
param(
    [ValidatePattern('^[A-Z]{3}$')]
    [string] $BaseCurrency = 'EUR'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$headers = @{
    Accept = 'application/json'
    'User-Agent' = 'Persistent-Calculator-Frankfurter-Contract-Check/1.0'
}

$metadataUri = 'https://api.frankfurter.dev/v2/currencies'
$ratesUri = "https://api.frankfurter.dev/v2/rates?base=$BaseCurrency"

Write-Host 'Reading Frankfurter currency metadata once...'
$metadata = Invoke-RestMethod -Uri $metadataUri -Headers $headers -TimeoutSec 30
Write-Host 'Reading Frankfurter current rates once...'
$rates = Invoke-RestMethod -Uri $ratesUri -Headers $headers -TimeoutSec 30

if ($metadata.Count -lt 100) {
    throw "Frankfurter returned only $($metadata.Count) currency records."
}

if ($rates.Count -lt 100) {
    throw "Frankfurter returned only $($rates.Count) rate records."
}

$invalidMetadata = @(
    $metadata | Where-Object {
        [string]::IsNullOrWhiteSpace($_.iso_code) -or
        $_.iso_code -notmatch '^[A-Z]{3}$' -or
        [string]::IsNullOrWhiteSpace($_.name)
    }
)
if ($invalidMetadata.Count -gt 0) {
    throw "Frankfurter returned $($invalidMetadata.Count) invalid currency metadata records."
}

$duplicateMetadata = @($metadata | Group-Object iso_code | Where-Object Count -gt 1)
if ($duplicateMetadata.Count -gt 0) {
    throw "Frankfurter returned duplicate currency codes: $(($duplicateMetadata.Name | Sort-Object) -join ', ')"
}

$invalidRates = @(
    $rates | Where-Object {
        [string]::IsNullOrWhiteSpace($_.base) -or
        [string]::IsNullOrWhiteSpace($_.quote) -or
        $_.base -notmatch '^[A-Z]{3}$' -or
        $_.quote -notmatch '^[A-Z]{3}$' -or
        [double]$_.rate -le 0 -or
        [double]::IsNaN([double]$_.rate) -or
        [double]::IsInfinity([double]$_.rate)
    }
)
if ($invalidRates.Count -gt 0) {
    throw "Frankfurter returned $($invalidRates.Count) invalid rate records."
}

$duplicateRates = @($rates | Group-Object quote | Where-Object Count -gt 1)
if ($duplicateRates.Count -gt 0) {
    throw "Frankfurter returned duplicate quote currencies: $(($duplicateRates.Name | Sort-Object) -join ', ')"
}

$metadataCodes = @($metadata.iso_code | Sort-Object -Unique)
$rateCodes = @($rates.quote | Sort-Object -Unique)
$ratesWithoutMetadata = @($rateCodes | Where-Object { $_ -notin $metadataCodes })
if ($ratesWithoutMetadata.Count -gt 0) {
    throw "Current rates have no matching metadata: $($ratesWithoutMetadata -join ', ')"
}

$selectableCodes = @($metadataCodes | Where-Object { $_ -eq $BaseCurrency -or $_ -in $rateCodes })
$metadataWithoutRate = @($metadataCodes | Where-Object { $_ -ne $BaseCurrency -and $_ -notin $rateCodes })

[pscustomobject]@{
    BaseCurrency = $BaseCurrency
    MetadataCurrencies = $metadataCodes.Count
    RateQuotes = $rateCodes.Count
    SelectableCurrencies = $selectableCodes.Count
    MetadataExcludedWithoutCurrentRate = $metadataWithoutRate -join ', '
    OldestContributingRateDate = $rates.date | Sort-Object | Select-Object -First 1
    NewestContributingRateDate = $rates.date | Sort-Object -Descending | Select-Object -First 1
} | Format-List

Write-Host 'Frankfurter currency metadata and current rates are internally consistent.' -ForegroundColor Green
