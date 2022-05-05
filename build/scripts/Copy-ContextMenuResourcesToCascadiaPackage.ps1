# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

$LocalizationsFromContextMenu = Get-ChildItem ./src/cascadia/TerminalApp/Resources -Recurse -Filter ContextMenu.resw
$Languages = [System.Collections.HashTable]::New()
$LocalizationsFromContextMenu | ForEach-Object {
	$Languages[$_.Directory.Name] = $_
}

ForEach ($pair in $Languages.GetEnumerator()) {
	$LanguageDir = "./src/cascadia/CascadiaPackage/Resources/$($pair.Key)"
	$ResPath = "$LanguageDir/Resources.resw"
	$PreexistingResw = Get-Item $ResPath -EA:Ignore
	If ($null -eq $PreexistingResw) {
		Write-Host "Copying $($pair.Value.FullName) to $ResPath"
		New-Item -type Directory $LanguageDir -EA:Ignore
		Copy-Item $pair.Value.FullName $ResPath
	} Else {
		# Merge Them!
		Write-Host "Merging $($pair.Value.FullName) into $ResPath"
		$existingXml = [xml](Get-Content $PreexistingResw.FullName)
		$newXml = [xml](Get-Content $pair.Value.FullName)
		$newDataKeys = $newXml.root.data.name
		$existingXml.root.data | % {
			If ($_.name -in $newDataKeys) {
				$null = $existingXml.root.RemoveChild($_)
			}
		}
		$newXml.root.data | % {
			$null = $existingXml.root.AppendChild($existingXml.ImportNode($_, $true))
		}
		$existingXml.Save($PreexistingResw.FullName)
	}
}
