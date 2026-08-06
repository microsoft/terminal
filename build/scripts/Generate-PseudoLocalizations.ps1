Get-ChildItem -Recurse -Directory -Filter qps-ploc*
    | Get-ChildItem -Include *.resw,*.xml
    | ForEach-Object {
        $source = Join-Path $_.Directory "../en-US/$($_.Name)"
        $target = $_

        $ploc = ./tools/ConvertTo-PseudoLocalization.ps1 -Path $source

        $writerSettings = [System.Xml.XmlWriterSettings]::new()
        $writerSettings.NewLineChars = "`r`n"
        $writerSettings.Indent = $true
        $writerSettings.Encoding = [System.Text.UTF8Encoding]::new($false) # suppress the BOM
        $writer = [System.Xml.XmlWriter]::Create($target, $writerSettings)
        $ploc.Save($writer)
        $writer.Flush()
        $writer.Close()
    }
