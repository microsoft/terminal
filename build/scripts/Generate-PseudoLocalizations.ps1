Get-ChildItem -Recurse -Filter *.resw
    | Where-Object { $_.Directory.Name.StartsWith("qps-ploc") }
    | ForEach-Object {
        $source = Join-Path $_.Directory "../en-US/$($_.Name)"
        $target = $_

        $ploc = ./tools/ConvertTo-PseudoLocalization.ps1 -Path $source

        $writerSettings = [System.Xml.XmlWriterSettings]::new()
        $writerSettings.NewLineChars = "`r`n"
        $writerSettings.Indent = $true
        $writer = [System.Xml.XmlWriter]::Create($target, $writerSettings)
        $ploc.Save($writer)
        $writer.Flush()
        $writer.Close()
    }
