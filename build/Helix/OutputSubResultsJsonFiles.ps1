Param(
    [Parameter(Mandatory = $true)] 
    [string]$WttInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$WttSingleRerunInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$WttMultipleRerunInputPath,

    [Parameter(Mandatory = $true)] 
    [string]$TestNamePrefix
)

# Ideally these would be passed as parameters to the script. However ps makes it difficult to deal with string literals containing '&', so we just 
# read the values directly from the environment variables
$helixResultsContainerUri = $Env:HELIX_RESULTS_CONTAINER_URI
$helixResultsContainerRsas = $Env:HELIX_RESULTS_CONTAINER_RSAS

Add-Type -Language CSharp -ReferencedAssemblies System.Xml,System.Xml.Linq,System.Runtime.Serialization,System.Runtime.Serialization.Json (Get-Content $PSScriptRoot\HelixTestHelpers.cs -Raw)

$testResultParser = [HelixTestHelpers.TestResultParser]::new($TestNamePrefix, $helixResultsContainerUri, $helixResultsContainerRsas)
[System.Collections.Generic.Dictionary[string, string]]$subResultsJsonByMethodName = $testResultParser.GetSubResultsJsonByMethodName($WttInputPath, $WttSingleRerunInputPath, $WttMultipleRerunInputPath)

$subResultsJsonDirectory = [System.IO.Path]::GetDirectoryName($WttInputPath)

foreach ($methodName in $subResultsJsonByMethodName.Keys)
{
    $subResultsJson = $subResultsJsonByMethodName[$methodName]
    $subResultsJsonPath = [System.IO.Path]::Combine($subResultsJsonDirectory, $methodName + "_subresults.json")
    Out-File $subResultsJsonPath -Encoding utf8 -InputObject $subResultsJson
}