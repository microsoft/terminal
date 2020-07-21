Param(
    [Parameter(Mandatory = $true)] 
    [string]$WttInputPath
)

Add-Type -Language CSharp -ReferencedAssemblies System.Xml,System.Xml.Linq,System.Runtime.Serialization,System.Runtime.Serialization.Json (Get-Content $PSScriptRoot\HelixTestHelpers.cs -Raw)

[HelixTestHelpers.FailedTestDetector]::OutputFailedTestQuery($WttInputPath)