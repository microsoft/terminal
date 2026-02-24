Param(
    [Parameter(Mandatory = $true)]
    [string]$XUnitOutputPath
)

# This script is used to parse the XUnit output from the test runs and print out
# the tests that failed.
#
# Why you might ask? Well, it sure seems like Azure DevOps doesn't like the fact
# that we just call our tests in a powershell script. It can't seemingly find
# the actual errors in the TAEF logs. That means when you just go to the
# "Checks" page on GitHub, the Azure DevOps integration doesn't have anything
# meaningful to say other than "PowerShell exited with code '1'". If we however,
# just manually emit the test names formatted with "#[error]" in front of them,
# well, then the integration will all work like magic.

# Load the test results as a XML object
$testResults = [xml](Get-Content -Path $XUnitOutputPath)

# Our XML looks like:
# <assemblies>
#   <assembly name="MUXControls.Test.dll" test-framework="TAEF" run-date="2023-08-14" run-time="11:38:01" total="524" passed="520" failed="4" skipped="1" time="8943" errors="0">
#     <collection total="524" passed="520" failed="4" skipped="1" name="Test collection" time="8943">
#       <test name="ControlCoreTests::TestSimpleClickSelection" type="ControlCoreTests" method="TestSimpleClickSelection" time="0.016" result="Fail">

# Iterate over all the assemblies and print all the tests that failed
foreach ($assembly in $testResults.assemblies.assembly) {
    foreach ($collection in $assembly.collection) {
        foreach ($test in $collection.test) {
            if ($test.result -eq "Fail") {
                # This particular format is taken from the Azure DevOps documentation:
                # https://github.com/microsoft/azure-pipelines-tasks/blob/master/docs/authoring/commands.md
                # This will treat this line as an error message
                Write-Output "##vso[task.logissue type=error]$($test.name) Failed"
            }
        }
    }
}
