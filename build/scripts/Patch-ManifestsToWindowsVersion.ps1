# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

Param(
	[string]$NewWindowsVersion = "10.0.22000.0"
)

Get-ChildItem src/cascadia/CascadiaPackage -Recurse -Filter *.appxmanifest | ForEach-Object {
	$xml = [xml](Get-Content $_.FullName)
	$xml.Package.Dependencies.TargetDeviceFamily | Where-Object Name -Like "Windows*" | ForEach-Object {
		$_.MinVersion = $NewWindowsVersion
	}
	$xml.Save($_.FullName)
}
