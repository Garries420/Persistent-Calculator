param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$VirusTotalText,

    [string]$VirusTotalUrl = '',

    [Parameter(Mandatory = $true)]
    [string]$KasperskyText,

    [string]$KasperskyUrl = '',

    [string]$ReadmePath = 'README.md'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function ConvertTo-HtmlText([string]$Value) {
    return [System.Net.WebUtility]::HtmlEncode($Value)
}

function New-ScannerBadgeHtml(
    [string]$Label,
    [string]$Status,
    [string]$Color,
    [string]$Url
) {
    $badgeLabel = [Uri]::EscapeDataString($Label.Replace('-', '--'))
    $badgeStatus = [Uri]::EscapeDataString($Status.Replace('-', '--'))
    $safeText = ConvertTo-HtmlText "$Label $Status"
    $badgeUrl = "https://img.shields.io/badge/$badgeLabel-$badgeStatus-$Color"
    $content = "<img src=`"$badgeUrl`" alt=`"$safeText`">"
    if ([string]::IsNullOrWhiteSpace($Url)) {
        return $content
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Url, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw "Scanner report URL must be an absolute HTTPS URL."
    }

    $safeUrl = ConvertTo-HtmlText $uri.AbsoluteUri
    return "<a href=`"$safeUrl`">$content</a>"
}

$readme = Get-Content -LiteralPath $ReadmePath -Raw
if ($VirusTotalText -notmatch '^VirusTotal\s+(?<status>.+)$') {
    throw "Unexpected VirusTotal status text: $VirusTotalText"
}
$virusTotalStatus = $Matches.status
if ($KasperskyText -notmatch '^Kaspersky OpenTIP:\s*(?<status>.+)$') {
    throw "Unexpected Kaspersky OpenTIP status text: $KasperskyText"
}
$kasperskyStatus = $Matches.status

$virusTotal = New-ScannerBadgeHtml 'VirusTotal' $virusTotalStatus '394eff' $VirusTotalUrl
$kaspersky = New-ScannerBadgeHtml 'Kaspersky OpenTIP' $kasperskyStatus '00a88e' $KasperskyUrl

$replacement = @"
<!-- security-status:start -->
  <a href="https://github.com/Garries420/Persistent-Calculator/releases/latest"><img src="https://img.shields.io/badge/release-v$Version-7c4dff" alt="release v$Version"></a>
  <img src="https://img.shields.io/badge/platform-Windows-1674ea" alt="platform Windows">
  $virusTotal
  $kaspersky
<!-- security-status:end -->
"@

$markerPattern = '(?s)<!-- security-status:start -->.*?<!-- security-status:end -->'
if ($readme -match $markerPattern) {
    $updated = [regex]::Replace($readme, $markerPattern, $replacement.Trim())
}
else {
    $oldHeaderPattern = '(?m)^  <a href="https://github\.com/Garries420/Persistent-Calculator/releases/latest"><img src="https://img\.shields\.io/badge/release-v[^\"]+" alt="release v[^\"]+"></a>\r?\n  <img src="https://img\.shields\.io/badge/platform-Windows-1674ea" alt="platform Windows">'
    if ($readme -notmatch $oldHeaderPattern) {
        throw 'The README header could not be located. No file was changed.'
    }
    $updated = [regex]::Replace($readme, $oldHeaderPattern, $replacement.Trim())
}

[System.IO.File]::WriteAllText(
    (Join-Path (Get-Location) $ReadmePath),
    $updated,
    [System.Text.UTF8Encoding]::new($false)
)
