$projects = Get-Childitem -Path .\ -Filter *.vcxproj -File
if ($projects.length -eq 0)
{
    exit -1
}
$projectPath = $projects.FullName
# exit
# param([String]$projectPath)

# $projectPath = "c:\Users\migrie\dev\private\OpenConsole\src\cascadia\WindowsTerminal\WindowsTerminal.vcxproj"
# $projectPath = $1
$msBuildCondition = "'%(ProjectReference.Identity)' == '$projectPath.metaproj'"
# $msBuildCondition = "'%(ProjectReference.Identity)' == '" $projectPath ".metaproj'"
# Write-Host $msBuildCondition

[xml]$Metaproj = Get-Content "$env:OPENCON\OpenConsole.sln.metaproj"

$thing = $Metaproj.Project.PropertyGroup.MSBuildRuntimeVersion
# Write-Host $thing

$targets = $Metaproj.Project.Target
# Write-Host $targets

# # run selected tests
# foreach ($t in $targets)
# {
#     # Write-Host $t.Name
# }

$matchingTargets = $targets | Where-Object { $_.MSBuild.Condition -eq $msBuildCondition }
# Write-Host $matchingTargets.Name
# Write-Host $matchingTargets.MsBuild.Targets

$matchingTargets = $matchingTargets | Where-Object { $hasProperty = $_.MsBuild.PSobject.Properties.name -match "Targets" ; return -Not $hasProperty }
# $matchingTargets = $matchingTargets | Where-Object { $hasProperty = $_.Targets -eq $null }
Write-Host $matchingTargets.Name
exit 0
