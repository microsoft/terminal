Param(
    [Parameter(Mandatory = $true)] 
    [int]$MinimumExpectedTestsExecutedCount,

    [string]$AccessToken = $env:SYSTEM_ACCESSTOKEN,
    [string]$CollectionUri = $env:SYSTEM_COLLECTIONURI,
    [string]$TeamProject = $env:SYSTEM_TEAMPROJECT,
    [string]$BuildUri = $env:BUILD_BUILDURI,
    [bool]$CheckJobAttempt
)

$azureDevOpsRestApiHeaders = @{
    "Accept"="application/json"
    "Authorization"="Basic $([System.Convert]::ToBase64String([System.Text.ASCIIEncoding]::ASCII.GetBytes(":$AccessToken")))"
}

. "$PSScriptRoot/AzurePipelinesHelperScripts.ps1"

Write-Host "Checking test results..."

$queryUri = GetQueryTestRunsUri -CollectionUri $CollectionUri -TeamProject $TeamProject -BuildUri $BuildUri -IncludeRunDetails
Write-Host "queryUri = $queryUri"

$testRuns = Invoke-RestMethod -Uri $queryUri -Method Get -Headers $azureDevOpsRestApiHeaders
[System.Collections.Generic.List[string]]$failingTests = @()
[System.Collections.Generic.List[string]]$unreliableTests = @()
[System.Collections.Generic.List[string]]$unexpectedResultTest = @()

[System.Collections.Generic.List[string]]$namesOfProcessedTestRuns = @()
$totalTestsExecutedCount = 0

# We assume that we only have one testRun with a given name that we care about
# We only process the last testRun with a given name (based on completedDate)
# The name of a testRun is set to the Helix queue that it was run on (e.g. windows.10.amd64.client19h1.xaml)
# If we have multiple test runs on the same queue that we care about, we will need to re-visit this logic
foreach ($testRun in ($testRuns.value | Sort-Object -Property "completedDate" -Descending))
{
    if ($CheckJobAttempt)
    {
        if ($namesOfProcessedTestRuns -contains $testRun.name)
        {
            Write-Host "Skipping test run '$($testRun.name)', since we have already processed a test run of that name."
            continue
        }
    }
    
    Write-Host "Processing results from test run '$($testRun.name)'"
    $namesOfProcessedTestRuns.Add($testRun.name)

    $totalTestsExecutedCount += $testRun.totalTests

    $testRunResultsUri = "$($testRun.url)/results?api-version=5.0"
    $testResults = Invoke-RestMethod -Uri "$($testRun.url)/results?api-version=5.0" -Method Get -Headers $azureDevOpsRestApiHeaders
        
    foreach ($testResult in $testResults.value)
    {
        $shortTestCaseTitle = $testResult.testCaseTitle -replace "[a-zA-Z0-9]+.[a-zA-Z0-9]+.Windows.UI.Xaml.Tests.MUXControls.",""

        if ($testResult.outcome -eq "Failed")
        {
            if (-not $failingTests.Contains($shortTestCaseTitle))
            {
                $failingTests.Add($shortTestCaseTitle)
            }
        }
        elseif ($testResult.outcome -eq "Warning")
        {
            if (-not $unreliableTests.Contains($shortTestCaseTitle))
            {
                $unreliableTests.Add($shortTestCaseTitle)
            }
        }
        elseif ($testResult.outcome -ne "Passed")
        {
            # We should only see tests with result "Passed", "Failed" or "Warning"
            if (-not $unexpectedResultTest.Contains($shortTestCaseTitle))
            {
                $unexpectedResultTest.Add($shortTestCaseTitle)
            }
        }
    }
}

if ($unreliableTests.Count -gt 0)
{
    Write-Host @"
##vso[task.logissue type=warning;]Unreliable tests:
##vso[task.logissue type=warning;]$($unreliableTests -join "$([Environment]::NewLine)##vso[task.logissue type=warning;]")

"@
}

if ($failingTests.Count -gt 0)
{
    Write-Host @"
##vso[task.logissue type=error;]Failing tests:
##vso[task.logissue type=error;]$($failingTests -join "$([Environment]::NewLine)##vso[task.logissue type=error;]")

"@
}

if ($unexpectedResultTest.Count -gt 0)
{
    Write-Host @"
##vso[task.logissue type=error;]Tests with unexpected results:
##vso[task.logissue type=error;]$($unexpectedResultTest -join "$([Environment]::NewLine)##vso[task.logissue type=error;]")

"@
}

if($totalTestsExecutedCount -lt $MinimumExpectedTestsExecutedCount)
{
    Write-Host "Expected at least $MinimumExpectedTestsExecutedCount tests to be executed."
    Write-Host "Actual executed test count is: $totalTestsExecutedCount"
    Write-Host "##vso[task.complete result=Failed;]"
}
elseif ($failingTests.Count -gt 0)
{
    Write-Host "At least one test failed."
    Write-Host "##vso[task.complete result=Failed;]"
}
elseif ($unreliableTests.Count -gt 0)
{
    Write-Host "All tests eventually passed, but some initially failed."
    Write-Host "##vso[task.complete result=Succeeded;]"
}
else
{
    Write-Host "All tests passed."
    Write-Host "##vso[task.complete result=Succeeded;]"
}
