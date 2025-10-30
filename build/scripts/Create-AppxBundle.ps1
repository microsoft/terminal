Param(
    [Parameter(Mandatory,
      HelpMessage="Base name for input .appx files")]
    [string]
    $ProjectName,

    [Parameter(Mandatory,
      HelpMessage="Appx Bundle Version")]
    [version]
    $BundleVersion,

    [Parameter(Mandatory,
      HelpMessage="Path under which to locate appx/msix files")]
    [string]
    $InputPath,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath,

    [Parameter(HelpMessage="Path to makeappx.exe")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakeAppxPath
)

If (-not $MakeAppxPath) {
  $winSdk10Root = $(Get-ItemPropertyValue -Path "HKLM:\Software\Microsoft\Windows Kits\Installed Roots" -Name "KitsRoot10")
  $MakeAppxPath = "$winSdk10Root\bin\10.0.22621.0\x86\MakeAppx.exe"
}

If ($null -Eq (Get-Item $MakeAppxPath -EA:SilentlyContinue)) {
    Write-Error "Could not find MakeAppx.exe at `"$MakeAppxPath`".`nMake sure that -MakeAppxPath points to a valid SDK."
    Exit 1
}

# Enumerates a set of appx files beginning with a project name
# and generates a temporary file containing a bundle content map.
Function Create-AppxBundleMapping {
    Param(
        [Parameter(Mandatory)]
        [string]
        $InputPath,

        [Parameter(Mandatory)]
        [string]
        $ProjectName
    )

    $lines = @("[Files]")
    Get-ChildItem -Path:$InputPath -Recurse -Filter:*$ProjectName* -Include *.appx, *.msix | % {
        $lines += ("`"{0}`" `"{1}`"" -f ($_.FullName, $_.Name))
    }
    $outputFile = New-TemporaryFile
    $lines | Out-File -Encoding:ASCII $outputFile
    $outputFile
}

$NewMapping = Create-AppxBundleMapping -InputPath:$InputPath -ProjectName:$ProjectName

& $MakeAppxPath bundle /v /bv $BundleVersion.ToString() /f $NewMapping.FullName /p $OutputPath
