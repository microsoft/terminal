# This is a helper script to figure out which target corresponds to the project
# in this directory. Parses the solution's .metaproj file looking for the
# project file in this directory, to be able to get the project's name.

$projects = Get-Childitem -Path .\ -Filter *.vcxproj -File
if ($projects.length -eq 0)
{
    exit -1
}
$projectPath = $projects.FullName


# Parse the solution's metaproj file.
[xml]$Metaproj = Get-Content "$env:OPENCON\OpenConsole.sln.metaproj"

$targets = $Metaproj.Project.Target

# Most projects are in OpenConsole.sln.metaproj as "<project>.*proj.metaproj".
# We'll filter to search for these first and foremost.
$msBuildCondition = "'%(ProjectReference.Identity)' == '$projectPath.metaproj'"

# Filter to project targets that match our metaproj file.
# For Conhost\Server, this will match:
#   [Conhost\Server, Conhost\Server:Clean, Conhost\Server:Rebuild, Conhost\Server:Publish]
$matchingTargets = $targets | Where-Object { $_.MSBuild.Condition -eq $msBuildCondition }

# If we didn't find a target, it's possible that the project didn't have a
# .metaproj in OpenConsole.sln.metaproj. Try filtering again, but leave off the
# .metaproj extension.
if ($matchingTargets.length -eq 0)
{
    $conditionNoMeta = "'%(ProjectReference.Identity)' == '$projectPath'"
    $matchingTargets = $targets | Where-Object { $_.MSBuild.Condition -eq $conditionNoMeta }
}

# Further filter to the targets that dont have a suffix (like ":Clean")
$matchingTargets = $matchingTargets | Where-Object { $hasProperty = $_.MsBuild.PSobject.Properties.name -match "Targets" ; return -Not $hasProperty }

Write-Host $matchingTargets.Name
exit 0
