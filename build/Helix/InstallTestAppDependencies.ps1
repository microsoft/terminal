# Displaying progress is unnecessary and is just distracting.
$ProgressPreference = "SilentlyContinue"

$dependencyFiles = Get-ChildItem -Filter "*Microsoft.VCLibs.*.appx"

foreach ($file in $dependencyFiles)
{
    Write-Host "Adding dependency $($file)..."

    Add-AppxPackage $file
    
}