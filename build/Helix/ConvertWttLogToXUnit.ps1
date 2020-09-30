Param(
    [Parameter(Mandatory = $true)] 
    [string]$WttInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$WttSingleRerunInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$WttMultipleRerunInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$XUnitOutputPath,

    [Parameter(Mandatory = $true)] 
    [string]$TestNamePrefix
)

# Ideally these would be passed as parameters to the script. However ps makes it difficult to deal with string literals containing '&', so we just 
# read the values directly from the environment variables
$helixResultsContainerUri = $Env:HELIX_RESULTS_CONTAINER_URI
$helixResultsContainerRsas = $Env:HELIX_RESULTS_CONTAINER_RSAS

$rerunPassesRequiredToAvoidFailure = $env:rerunPassesRequiredToAvoidFailure

Add-Type -Language CSharp -ReferencedAssemblies System.Xml,System.Xml.Linq,System.Runtime.Serialization,System.Runtime.Serialization.Json (Get-Content $PSScriptRoot\HelixTestHelpers.cs -Raw)

$testResultParser = [HelixTestHelpers.TestResultParser]::new($TestNamePrefix, $helixResultsContainerUri, $helixResultsContainerRsas)
$testResultParser.ConvertWttLogToXUnitLog($WttInputPath, $WttSingleRerunInputPath, $WttMultipleRerunInputPath, $XUnitOutputPath, $rerunPassesRequiredToAvoidFailure)