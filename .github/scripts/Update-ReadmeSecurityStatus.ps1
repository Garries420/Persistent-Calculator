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

function New-ScannerStatusHtml(
    [string]$IconPath,
    [string]$Text,
    [string]$Url
) {
    $safeText = ConvertTo-HtmlText $Text
    $content = "<img src=`"$IconPath`" alt=`"`" width=`"15`" height=`"15`" align=`"absmiddle`"> $safeText"
    if ([string]::IsNullOrWhiteSpace($Url)) {
        return "<span>$content</span>"
    }

    $uri = $null
    if (-not [Uri]::TryCreate($Url, [UriKind]::Absolute, [ref]$uri) -or $uri.Scheme -ne 'https') {
        throw "Scanner report URL must be an absolute HTTPS URL."
    }

    $safeUrl = ConvertTo-HtmlText $uri.AbsoluteUri
    return "<a href=`"$safeUrl`">$content</a>"
}

$readme = Get-Content -LiteralPath $ReadmePath -Raw
$virusTotal = New-ScannerStatusHtml 'docs/Images/virustotal-shield.svg' $VirusTotalText $VirusTotalUrl
$kaspersky = New-ScannerStatusHtml 'docs/Images/kaspersky-shield.svg' $KasperskyText $KasperskyUrl

$replacement = @"
<!-- security-status:start -->
  <a href="https://github.com/Garries420/Persistent-Calculator/releases/latest"><img src="https://img.shields.io/badge/release-v$Version-7c4dff" alt="release v$Version"></a>
  &nbsp;&middot;&nbsp;
  <img src="https://img.shields.io/badge/platform-Windows-1674ea" alt="platform Windows">
  &nbsp;&middot;&nbsp;
  $virusTotal
  &nbsp;&middot;&nbsp;
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
