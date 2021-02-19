#
# Generate-TerminalAssets.ps1
#
# Typical usage:
#   .\Generate-TerminalAssets.ps1 -Path .\Terminal.svg -HighContrastPath .\Terminal_HC.svg -Destination .\images
#   .\Generate-TerminalAssets.ps1 -Path .\Terminal_Pre.svg -HighContrastPath .\Terminal_Pre_HC.svg -Destination .\images-Pre
#   .\Generate-TerminalAssets.ps1 -Path .\Terminal_Dev.svg -HighContrastPath .\Terminal_Dev_HC.svg -Destination .\images-Dev
#
# Some icons benefit from manual hints.  The most efficient way to do that is to run the script twice:
#
#   1. Run .\Generate-TerminalAssets.ps1 ...args... -Destination .\images -KeepIntermediates
#   2. Manually hint the intermediate images under .\images\_intermediate*.png
#   3. Run .\Generate-TerminalAssets.ps1 ...args... -Destination .\images -UseExistingIntermediates
#
# Hinting the intermediate files minimizes the number of times you'll have to
# hint the same image.  You may want to hint just the _intermediate.*.png and
# _intermediate.black.*.png files, and delete _intermediate.white.*.png.  The
# script will then automatically derive _intermediate.white.*.png from
# _intermediate.black.*.png.
#

Param(
  [Parameter(Mandatory=$true,ValueFromPipeline=$true)]
  [string]$Path,
  [string]$Destination,
  [int[]]$Altforms = (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256),
  [int[]]$Win32IconSizes = (16, 20, 24, 32, 48, 64, 256),
  [switch]$Unplated = $true,
  [float[]]$Scales = (1.0, 1.25, 1.5, 2.0, 4.0),
  [string]$HighContrastPath = "",
  [switch]$UseExistingIntermediates = $false,
  [switch]$KeepIntermediates = $false
)

$assetTypes = @(
  [pscustomobject]@{Name="LargeTile"; W=310; H=310; IconSize=96}
  [pscustomobject]@{Name="LockScreenLogo"; W=24; H=24; IconSize=24}
  [pscustomobject]@{Name="SmallTile"; W=71; H=71; IconSize=36}
  [pscustomobject]@{Name="SplashScreen"; W=620; H=300; IconSize=96}
  [pscustomobject]@{Name="Square44x44Logo"; W=44; H=44; IconSize=32}
  [pscustomobject]@{Name="Square150x150Logo"; W=150; H=150; IconSize=48}
  [pscustomobject]@{Name="StoreLogo"; W=50; H=50; IconSize=36}
  [pscustomobject]@{Name="Wide310x150Logo"; W=310; H=150; IconSize=48}
)

function CeilToEven ([int]$i) { if ($i % 2 -eq 0) { [int]($i) } else { [int]($i + 1) } }

$inflatedAssetSizes = $assetTypes | ForEach-Object {
  $as = $_;
  $scales | ForEach-Object {
    [pscustomobject]@{
      Name = $as.Name + ".scale-$($_*100)"
      W = [math]::Round($as.W * $_, [System.MidpointRounding]::ToPositiveInfinity)
      H = [math]::Round($as.H * $_, [System.MidpointRounding]::ToPositiveInfinity)
      IconSize = CeilToEven ($as.IconSize * $_)
    }
  }
}

$allAssetSizes = $inflatedAssetSizes + ($Altforms | ForEach-Object {
    [pscustomobject]@{
      Name = "Square44x44Logo.targetsize-${_}"
      W = [int]$_
      H = [int]$_
      IconSize = [int]$_
    }
    If ($Unplated) {
      [pscustomobject]@{
        Name = "Square44x44Logo.targetsize-${_}_altform-unplated"
        W = [int]$_
        H = [int]$_
        IconSize = [int]$_
      }
    }
  })

# Cross product with the 3 high contrast modes
$allAssetSizes = $allAssetSizes | ForEach-Object {
    $asset = $_
    ("standard", "black", "white") | ForEach-Object {
        $contrast = $_
        $name = $asset.Name
        If ($contrast -Ne "standard") {
            If ($HighContrastPath -Eq "") {
                # "standard" is the default, so we can omit it in filenames
                return
            }
            $name += "_contrast-" + $contrast
        }
        [pscustomobject]@{
            Name = $name
            W = $asset.W
            H = $asset.H
            IconSize = $asset.IconSize
            Contrast = $_
      }
    }
}

$allSizes = $allAssetSizes.IconSize | Group-Object | Select-Object -Expand Name

$TranslatedSVGPath = & wsl wslpath -u ((Get-Item $Path -ErrorAction:Stop).FullName -Replace "\\","/")
$TranslatedSVGContrastPath = $null
If ($HighContrastPath -Ne "") {
    $TranslatedSVGContrastPath = & wsl wslpath -u ((Get-Item $HighContrastPath -ErrorAction:Stop).FullName -Replace "\\","/")
}
& wsl which inkscape | Out-Null
If ($LASTEXITCODE -Ne 0) { throw "Inkscape is not installed in WSL" }
& wsl which convert | Out-Null
If ($LASTEXITCODE -Ne 0) { throw "imagemagick is not installed in WSL" }

If (-Not [string]::IsNullOrEmpty($Destination)) {
  New-Item -Type Directory $Destination -EA:Ignore
  $TranslatedOutDir = & wsl wslpath -u ((Get-Item $Destination -EA:Stop).FullName -Replace "\\","/")
} Else {
  $TranslatedOutDir = "."
}

$intermediates = [System.Collections.Concurrent.ConcurrentBag[PSCustomObject]]::new()
$intermediateFiles = [System.Collections.Concurrent.ConcurrentBag[string]]::new()

# Generate the base icons
$allSizes | ForEach-Object -Parallel {
  $sz = $_;

  $destinationNt = $using:Destination
  $destinationWsl = $using:TranslatedOutDir
  $svgStandardWsl = $using:TranslatedSVGPath
  $svgContrastWsl = $using:TranslatedSVGContrastPath

  $intermediateStandardNt = "$destinationNt\_intermediate.standard.$($sz).png"
  $intermediateStandardWsl = "$destinationWsl/_intermediate.standard.$($sz).png"

  If (($using:UseExistingIntermediates -Eq $false) -Or (-Not (Test-Path $intermediateStandardNt))) {
    wsl inkscape -z -e "$intermediateStandardWsl" -w $sz -h $sz $svgStandardWsl 
  } Else {
    Write-Host "Using existing $intermediateStandardNt"
  }

  ($using:intermediateFiles).Add($intermediateStandardNt)
  ($using:intermediates).Add([PSCustomObject]@{
      Contrast = "standard"
      Size = $sz
      PathWSL = $intermediateStandardWsl
  })

  If ($svgContrastWsl -Ne $null) {
    $intermediateBlackNt = "$destinationNt\_intermediate.black.$($sz).png"
    $intermediateWhiteNt = "$destinationNt\_intermediate.white.$($sz).png"
    $intermediateBlackWsl = "$destinationWsl/_intermediate.black.$($sz).png"
    $intermediateWhiteWsl = "$destinationWsl/_intermediate.white.$($sz).png"

    If (($using:UseExistingIntermediates -Eq $false) -Or (-Not (Test-Path $intermediateBlackNt))) {
      wsl inkscape -z -e "$intermediateBlackWsl" -w $sz -h $sz $svgContrastWsl 
    } Else {
      Write-Host "Using existing $intermediateBlackNt"
    }

    If (($using:UseExistingIntermediates -Eq $false) -Or (-Not (Test-Path $intermediateWhiteNt))) {
      # The HC white icon is just a negative image of the HC black one
      wsl convert "$intermediateBlackWsl" -channel RGB -negate "$intermediateWhiteWsl"
    } Else {
      Write-Host "Using existing $intermediateWhiteNt"
    }

    ($using:intermediateFiles).Add($intermediateBlackNt)
    ($using:intermediateFiles).Add($intermediateWhiteNt)
    ($using:intermediates).Add([PSCustomObject]@{
        Contrast = "black"
        Size = $sz
        PathWSL = $intermediateBlackWsl
    })
    ($using:intermediates).Add([PSCustomObject]@{
        Contrast = "white"
        Size = $sz
        PathWSL = $intermediateWhiteWsl
    })
  }
}

$intermediates | ? { $_.Size -In $Win32IconSizes } | Group-Object Contrast | ForEach-Object -Parallel {
  $assetName = "terminal.ico"
  If ($_.Name -Ne "standard") {
    $assetName = "terminal_contrast-$($_.Name).ico"
  }
  Write-Host "Producing win32 .ico for contrast=$($_.Name) as $assetName"
  wsl convert $_.Group.PathWSL "$($using:TranslatedOutDir)/$assetName"
}

# Once the base icons are done, splat them into the middles of larger canvases.
$allAssetSizes | ForEach-Object -Parallel {
  $asset = $_
  If ($asset.W -Eq $asset.H -And $asset.IconSize -eq $asset.W) {
    Write-Host "Copying base icon for size=$($asset.IconSize), contrast=$($asset.Contrast) to $($asset.Name)"
    Copy-Item "${using:Destination}\_intermediate.$($asset.Contrast).$($asset.IconSize).png" "${using:Destination}\$($asset.Name).png" -Force
  } Else {
    wsl convert "$($using:TranslatedOutDir)/_intermediate.$($asset.Contrast).$($asset.IconSize).png" -gravity center -background transparent -extent "$($asset.W)x$($asset.H)" "$($using:TranslatedOutDir)/$($asset.Name).png"
  }
}

If ($KeepIntermediates -Eq $false) {
  $intermediateFiles | ForEach-Object {
    Write-Host "Cleaning up intermediate file $_"
    Remove-Item $_
  }
}

