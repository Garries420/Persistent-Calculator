param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [string]$VirusTotalApiKey = $env:VIRUSTOTAL_API_KEY,

    [string]$KasperskyApiToken = $env:KASPERSKY_OPENTIP_API_TOKEN,

    [string]$OutputPath = $env:GITHUB_OUTPUT,

    [int]$VirusTotalTimeoutMinutes = 20,

    [int]$KasperskyTimeoutMinutes = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath).Path
$sha256 = (Get-FileHash -LiteralPath $resolvedInstaller -Algorithm SHA256).Hash.ToLowerInvariant()

function Get-PropertyValue($Object, [string[]]$Names) {
    if ($null -eq $Object) {
        return $null
    }
    foreach ($name in $Names) {
        $property = $Object.PSObject.Properties[$name]
        if ($null -ne $property) {
            return $property.Value
        }
    }
    return $null
}

function Get-ReportSha256($Report) {
    $hash = Get-PropertyValue $Report @('Sha256', 'sha256')
    if ($null -eq $hash) {
        $general = Get-PropertyValue $Report @('FileGeneralInfo', 'fileGeneralInfo')
        $hash = Get-PropertyValue $general @('Sha256', 'sha256')
    }
    if ($null -eq $hash) {
        return ''
    }
    return ([string]$hash).ToLowerInvariant()
}

function Invoke-JsonRequest {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][hashtable]$Headers,
        [ValidateSet('Get', 'Post')][string]$Method = 'Get',
        [string]$InFile = '',
        [string]$ContentType = '',
        [hashtable]$Form = @{}
    )

    $request = @{
        Uri = $Uri
        Headers = $Headers
        Method = $Method
        SkipHttpErrorCheck = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($InFile)) {
        $request.InFile = $InFile
    }
    if (-not [string]::IsNullOrWhiteSpace($ContentType)) {
        $request.ContentType = $ContentType
    }
    if ($Form.Count -gt 0) {
        $request.Form = $Form
    }

    $response = Invoke-WebRequest @request
    $body = $null
    if (-not [string]::IsNullOrWhiteSpace($response.Content)) {
        try {
            $body = $response.Content | ConvertFrom-Json -Depth 100
        }
        catch {
            if ([int]$response.StatusCode -ge 200 -and [int]$response.StatusCode -lt 300) {
                throw "Scanner returned HTTP $([int]$response.StatusCode) with an invalid JSON response."
            }
        }
    }

    return [pscustomobject]@{
        StatusCode = [int]$response.StatusCode
        Body = $body
    }
}

function Get-VirusTotalReport([string]$Hash, [hashtable]$Headers) {
    $lookup = Invoke-JsonRequest -Uri "https://www.virustotal.com/api/v3/files/$Hash" -Headers $Headers
    if ($lookup.StatusCode -eq 404) {
        return $null
    }
    if ($lookup.StatusCode -ne 200) {
        throw "VirusTotal hash lookup failed with HTTP $($lookup.StatusCode)."
    }

    $fileId = [string](Get-PropertyValue $lookup.Body.data @('id'))
    if ($fileId.ToLowerInvariant() -ne $Hash) {
        throw 'VirusTotal returned a report for a different SHA-256.'
    }

    $results = Get-PropertyValue $lookup.Body.data.attributes @('last_analysis_results')
    if ($null -eq $results) {
        return $null
    }

    $relevantCategories = @('malicious', 'suspicious', 'undetected', 'harmless')
    $verdicts = @($results.PSObject.Properties | ForEach-Object { $_.Value })
    $relevantVerdicts = @($verdicts | Where-Object {
        $category = [string](Get-PropertyValue $_ @('category'))
        $relevantCategories -contains $category.ToLowerInvariant()
    })
    if ($relevantVerdicts.Count -eq 0) {
        return $null
    }

    $detections = @($relevantVerdicts | Where-Object {
        ([string](Get-PropertyValue $_ @('category'))).ToLowerInvariant() -eq 'malicious'
    }).Count

    return [pscustomobject]@{
        Text = "VirusTotal $detections/$($relevantVerdicts.Count)"
        Url = "https://www.virustotal.com/gui/file/$Hash/detection"
        Complete = $true
    }
}

function Submit-VirusTotalFile([string]$Path, [hashtable]$Headers) {
    $uploadUri = 'https://www.virustotal.com/api/v3/files'
    if ((Get-Item -LiteralPath $Path).Length -gt 32MB) {
        $uploadLookup = Invoke-JsonRequest -Uri 'https://www.virustotal.com/api/v3/files/upload_url' -Headers $Headers
        if ($uploadLookup.StatusCode -ne 200) {
            throw "VirusTotal upload URL request failed with HTTP $($uploadLookup.StatusCode)."
        }
        $uploadUri = [string](Get-PropertyValue $uploadLookup.Body @('data'))
        if (-not $uploadUri.StartsWith('https://', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'VirusTotal returned an invalid upload URL.'
        }
    }

    $upload = Invoke-JsonRequest -Uri $uploadUri -Headers $Headers -Method Post -Form @{ file = Get-Item -LiteralPath $Path }
    if ($upload.StatusCode -lt 200 -or $upload.StatusCode -ge 300) {
        throw "VirusTotal upload failed with HTTP $($upload.StatusCode)."
    }
    $analysisId = [string](Get-PropertyValue $upload.Body.data @('id'))
    if ([string]::IsNullOrWhiteSpace($analysisId)) {
        throw 'VirusTotal upload did not return an analysis identifier.'
    }
    return $analysisId
}

function Wait-VirusTotalAnalysis([string]$AnalysisId, [hashtable]$Headers, [int]$TimeoutMinutes) {
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $encodedId = [Uri]::EscapeDataString($AnalysisId)
    while ((Get-Date) -lt $deadline) {
        $analysis = Invoke-JsonRequest -Uri "https://www.virustotal.com/api/v3/analyses/$encodedId" -Headers $Headers
        if ($analysis.StatusCode -ne 200) {
            throw "VirusTotal analysis lookup failed with HTTP $($analysis.StatusCode)."
        }
        $status = [string](Get-PropertyValue $analysis.Body.data.attributes @('status'))
        if ($status -eq 'completed') {
            return $true
        }
        Start-Sleep -Seconds 20
    }
    return $false
}

function Get-VirusTotalStatus {
    if ([string]::IsNullOrWhiteSpace($VirusTotalApiKey)) {
        throw 'The VirusTotal API key is not available.'
    }
    $headers = @{ 'x-apikey' = $VirusTotalApiKey }
    $report = Get-VirusTotalReport $sha256 $headers
    if ($null -ne $report) {
        Write-Host 'Reused the completed VirusTotal report for the exact installer SHA-256.'
        return $report
    }

    Write-Host 'No completed VirusTotal report exists for this SHA-256; submitting the release installer once.'
    $analysisId = Submit-VirusTotalFile $resolvedInstaller $headers
    if (-not (Wait-VirusTotalAnalysis $analysisId $headers $VirusTotalTimeoutMinutes)) {
        return [pscustomobject]@{
            Text = 'VirusTotal Pending'
            Url = "https://www.virustotal.com/gui/file/$sha256/detection"
            Complete = $false
        }
    }

    $report = Get-VirusTotalReport $sha256 $headers
    if ($null -eq $report) {
        throw 'VirusTotal marked the analysis complete but did not return final engine verdicts.'
    }
    return $report
}

function Convert-KasperskyReportToStatus($Report) {
    $reportHash = Get-ReportSha256 $Report
    if ([string]::IsNullOrWhiteSpace($reportHash) -or $reportHash -ne $sha256) {
        return $null
    }

    $publicReportUrl = "https://opentip.kaspersky.com/$sha256/"
    $zone = ([string](Get-PropertyValue $Report @('Zone', 'zone'))).Trim()
    $fileStatus = ([string](Get-PropertyValue $Report @('FileStatus', 'fileStatus'))).Trim()
    # OpenTIP defines the Green zone itself as Clean or No threats detected.
    # The FileStatus field is not present in every otherwise complete response.
    if ($zone -eq 'Green' -or $fileStatus -in @('Clean', 'No threats detected')) {
        return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Clean'; Url = $publicReportUrl; Complete = $true }
    }
    if ($zone -eq 'Red' -or $fileStatus -eq 'Malware') {
        return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Malware'; Url = $publicReportUrl; Complete = $true }
    }
    if ($zone -eq 'Yellow' -or $fileStatus -eq 'Adware and other') {
        return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Adware and other'; Url = $publicReportUrl; Complete = $true }
    }
    return $null
}

function Get-KasperskyHashReport([hashtable]$Headers) {
    $encodedHash = [Uri]::EscapeDataString($sha256)
    $lookup = Invoke-JsonRequest -Uri "https://opentip.kaspersky.com/api/v1/search/hash?request=$encodedHash" -Headers $Headers
    if ($lookup.StatusCode -eq 404) {
        return $null
    }
    if ($lookup.StatusCode -ne 200) {
        throw "Kaspersky OpenTIP hash lookup failed with HTTP $($lookup.StatusCode)."
    }
    return Convert-KasperskyReportToStatus $lookup.Body
}

function Submit-KasperskyFile([hashtable]$Headers) {
    $encodedName = [Uri]::EscapeDataString((Split-Path -Leaf $resolvedInstaller))
    $submit = Invoke-JsonRequest `
        -Uri "https://opentip.kaspersky.com/api/v1/scan/file?filename=$encodedName" `
        -Headers $Headers `
        -Method Post `
        -InFile $resolvedInstaller `
        -ContentType 'application/octet-stream'
    if ($submit.StatusCode -ne 200) {
        throw "Kaspersky OpenTIP upload failed with HTTP $($submit.StatusCode)."
    }
    $returnedHash = Get-ReportSha256 $submit.Body
    if (-not [string]::IsNullOrWhiteSpace($returnedHash) -and $returnedHash -ne $sha256) {
        throw 'Kaspersky OpenTIP returned a result for a different SHA-256.'
    }
}

function Wait-KasperskyAnalysis([hashtable]$Headers, [int]$TimeoutMinutes) {
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    $encodedHash = [Uri]::EscapeDataString($sha256)
    while ((Get-Date) -lt $deadline) {
        $result = Invoke-JsonRequest `
            -Uri "https://opentip.kaspersky.com/api/v1/getresult/file?request=$encodedHash" `
            -Headers $Headers `
            -Method Post
        if ($result.StatusCode -ne 200) {
            throw "Kaspersky OpenTIP result lookup failed with HTTP $($result.StatusCode)."
        }

        $analysisState = ([string](Get-PropertyValue $result.Body @('Status', 'status'))).Trim().ToLowerInvariant()
        if ($analysisState -eq 'complete') {
            $status = Convert-KasperskyReportToStatus $result.Body
            if ($null -ne $status) {
                return $status
            }
            # The sandbox result can finish before all reputation fields appear
            # in this response. Re-read the exact hash reputation once before
            # reporting an unknown verdict.
            $status = Get-KasperskyHashReport $Headers
            if ($null -ne $status) {
                return $status
            }
            return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Unknown'; Url = ''; Complete = $false }
        }
        if ($analysisState -notin @('in progress', 'not started', '')) {
            return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Unknown'; Url = ''; Complete = $false }
        }
        Start-Sleep -Seconds 30
    }
    return [pscustomobject]@{ Text = 'Kaspersky OpenTIP: Pending'; Url = ''; Complete = $false }
}

function Get-KasperskyStatus {
    if ([string]::IsNullOrWhiteSpace($KasperskyApiToken)) {
        throw 'The Kaspersky OpenTIP API token is not available.'
    }
    $headers = @{ 'x-api-key' = $KasperskyApiToken }
    $report = Get-KasperskyHashReport $headers
    if ($null -ne $report) {
        Write-Host 'Reused the completed Kaspersky OpenTIP report for the exact installer SHA-256.'
        return $report
    }

    Write-Host 'No completed Kaspersky OpenTIP report exists for this SHA-256; submitting the release installer once.'
    Submit-KasperskyFile $headers
    return Wait-KasperskyAnalysis $headers $KasperskyTimeoutMinutes
}

$virusTotalStatus = $null
try {
    $virusTotalStatus = Get-VirusTotalStatus
}
catch {
    Write-Warning 'VirusTotal did not return a usable result. The README will not claim zero detections.'
    $virusTotalStatus = [pscustomobject]@{
        Text = 'VirusTotal Unavailable'
        Url = "https://www.virustotal.com/gui/file/$sha256/detection"
        Complete = $false
    }
}

$kasperskyStatus = $null
try {
    $kasperskyStatus = Get-KasperskyStatus
}
catch {
    Write-Warning 'Kaspersky OpenTIP did not return a usable result. The README will not claim the installer is clean.'
    $kasperskyStatus = [pscustomobject]@{
        Text = 'Kaspersky OpenTIP: Unavailable'
        Url = ''
        Complete = $false
    }
}

$outputs = [ordered]@{
    sha256 = $sha256
    virustotal_text = $virusTotalStatus.Text
    virustotal_url = $virusTotalStatus.Url
    virustotal_complete = ([bool]$virusTotalStatus.Complete).ToString().ToLowerInvariant()
    kaspersky_text = $kasperskyStatus.Text
    kaspersky_url = $kasperskyStatus.Url
    kaspersky_complete = ([bool]$kasperskyStatus.Complete).ToString().ToLowerInvariant()
}

foreach ($entry in $outputs.GetEnumerator()) {
    Write-Host "$($entry.Key): $($entry.Value)"
    if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
        "$($entry.Key)=$($entry.Value)" | Out-File -LiteralPath $OutputPath -Append -Encoding utf8
    }
}
