Param(
    [string]$AccessToken = $env:SYSTEM_ACCESSTOKEN,
    [string]$HelixAccessToken = $env:HelixAccessToken,
    [string]$CollectionUri = $env:SYSTEM_COLLECTIONURI,
    [string]$TeamProject = $env:SYSTEM_TEAMPROJECT,
    [string]$BuildUri = $env:BUILD_BUILDURI,
    [string]$OutputFolder = "HelixOutput"
)

$helixLinkFile = "$OutputFolder\LinksToHelixTestFiles.html"


function Generate-File-Links
{
    Param ([Array[]]$files,[string]$sectionName)
    if($files.Count -gt 0)
    {
        Out-File -FilePath $helixLinkFile -Append -InputObject "<div class=$sectionName>"
        Out-File -FilePath $helixLinkFile -Append -InputObject "<h4>$sectionName</h4>"
        Out-File -FilePath $helixLinkFile -Append -InputObject "<ul>"
        foreach($file in $files)
        {
            Out-File -FilePath $helixLinkFile -Append -InputObject "<li><a href=$($file.Link)>$($file.Name)</a></li>"
        }
        Out-File -FilePath $helixLinkFile -Append -InputObject "</ul>"
        Out-File -FilePath $helixLinkFile -Append -InputObject "</div>"
    }
}

#Create output directory
New-Item $OutputFolder -ItemType Directory

$azureDevOpsRestApiHeaders = @{
    "Accept"="application/json"
    "Authorization"="Basic $([System.Convert]::ToBase64String([System.Text.ASCIIEncoding]::ASCII.GetBytes(":$AccessToken")))"
}

. "$PSScriptRoot/AzurePipelinesHelperScripts.ps1"

$queryUri = GetQueryTestRunsUri -CollectionUri $CollectionUri -TeamProject $TeamProject -BuildUri $BuildUri -IncludeRunDetails
Write-Host "queryUri = $queryUri"

$testRuns = Invoke-RestMethodWithRetries $queryUri -Headers $azureDevOpsRestApiHeaders
$webClient = New-Object System.Net.WebClient
[System.Collections.Generic.List[string]]$workItems = @()

foreach ($testRun in $testRuns.value)
{
    Write-Host "testRunUri = $testRun.url"
    $testResults = Invoke-RestMethodWithRetries "$($testRun.url)/results?api-version=5.0" -Headers $azureDevOpsRestApiHeaders
    $isTestRunNameShown = $false

    foreach ($testResult in $testResults.value)
    {
        $info = ConvertFrom-Json $testResult.comment
        $helixJobId = $info.HelixJobId
        $helixWorkItemName = $info.HelixWorkItemName

        $workItem = "$helixJobId-$helixWorkItemName"

        Write-Host "Helix Work Item = $workItem"

        if (-not $workItems.Contains($workItem))
        {
            $workItems.Add($workItem)
            $filesQueryUri = "https://helix.dot.net/api/2019-06-17/jobs/$helixJobId/workitems/$helixWorkItemName/files$accessTokenParam"
            $files = Invoke-RestMethodWithRetries $filesQueryUri

            $screenShots = $files | where { $_.Name.EndsWith(".jpg") }
            $dumps = $files | where { $_.Name.EndsWith(".dmp") }
            $pgcFiles = $files | where { $_.Name.EndsWith(".pgc") }
            if ($screenShots.Count + $dumps.Count + $pgcFiles.Count -gt 0)
            {
                if(-Not $isTestRunNameShown)
                {
                    Out-File -FilePath $helixLinkFile -Append -InputObject "<h2>$($testRun.name)</h2>"
                    $isTestRunNameShown = $true
                }
                Out-File -FilePath $helixLinkFile -Append -InputObject "<h3>$helixWorkItemName</h3>"
                Generate-File-Links $screenShots "Screenshots"
                Generate-File-Links $dumps "CrashDumps"
                Generate-File-Links $pgcFiles "PGC files"
                $misc = $files | where { ($screenShots -NotContains $_) -And ($dumps -NotContains $_) -And ($visualTreeVerificationFiles -NotContains $_) -And ($pgcFiles -NotContains $_) }
                Generate-File-Links $misc "Misc"

                foreach($pgcFile in $pgcFiles)
                {
                    $flavorPath = $testResult.automatedTestName.Split('.')[0]
                    $archPath = $testResult.automatedTestName.Split('.')[1]
                    $fileName = $pgcFile.Name
                    $fullPath = "$OutputFolder\PGO\$flavorPath\$archPath"
                    $destination = "$fullPath\$fileName"

                    Write-Host "Copying $($pgcFile.Name) to $destination"

                    if (-Not (Test-Path $fullPath))
                    {
                        New-Item $fullPath -ItemType Directory
                    }

                    $link = $pgcFile.Link

                    Write-Host "Downloading $link to $destination"

                    Download-FileWithRetries $link $destination
                }
            }
        }
    }
}
