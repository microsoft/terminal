# Displaying progress is unnecessary and is just distracting.
$ProgressPreference = "SilentlyContinue"

$dependencyFiles = Get-ChildItem -Filter "*dependencies.txt"

foreach ($file in $dependencyFiles)
{
    Write-Host "Adding dependency $($file)..."

    foreach ($line in Get-Content $file)
    {
        Add-AppxPackage $line
    }
}