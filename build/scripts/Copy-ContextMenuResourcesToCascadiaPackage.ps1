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
	$XmlDocument = $null
	$PreexistingResw = Get-Item $ResPath -EA:Ignore
	If ($null -eq $PreexistingResw) {
		Write-Host "Copying $($pair.Value.FullName) to $ResPath"
		$XmlDocument = [xml](Get-Content $pair.Value.FullName)
		New-Item -type Directory $LanguageDir -EA:Ignore
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
		$XmlDocument = $existingXml # (which has been updated)
	}

	# Reset paths to be absolute (for .NET)
	$LanguageDir = (Get-Item $LanguageDir).FullName
	$ResPath = "$LanguageDir/Resources.resw"
	# Force the "new" and "preexisting" paths to serialize with XmlWriter,
	# to ensure consistency.
	$writerSettings = [System.Xml.XmlWriterSettings]::new()
	$writerSettings.NewLineChars = "`r`n"
	$writerSettings.Indent = $true
	$writerSettings.Encoding = [System.Text.UTF8Encoding]::new($false) # suppress the BOM
	$writer = [System.Xml.XmlWriter]::Create($ResPath, $writerSettings)
	$XmlDocument.Save($writer)
	$writer.Flush()
	$writer.Close()
}
