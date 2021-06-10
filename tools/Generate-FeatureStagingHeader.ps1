# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

################################################################################
# This script generates a header describing which Terminal/Console features
# should be compiled-in, based on an XML document describing them.

[CmdletBinding()]
Param(
    [Parameter(Position=0, Mandatory=$True)]
    [ValidateScript({ Test-Path $_ })]
    [string]$Path,

    [ValidateSet("Dev", "Preview", "Release", "WindowsInbox")]
    [string]$Branding = "Dev",

    [string]$BranchOverride = $Null,

    [string]$OutputPath
)

Enum Stage {
    AlwaysDisabled;
    AlwaysEnabled;
}

Function ConvertTo-FeatureStage([string]$stage) {
    Switch($stage) {
        "AlwaysEnabled" { [Stage]::AlwaysEnabled; Return }
        "AlwaysDisabled" { [Stage]::AlwaysDisabled; Return }
    }
    Throw "Invalid feature stage $stage"
}

Class Feature {
    [string]$Name
    [Stage]$Stage
    [System.Collections.Generic.Dictionary[string, Stage]]$BranchTokenStages
    [System.Collections.Generic.Dictionary[string, Stage]]$BrandingTokenStages
    [bool]$DisabledReleaseToken

    Feature([System.Xml.XmlElement]$entry) {
        $this.Name = $entry.name
        $this.Stage = ConvertTo-FeatureStage $entry.stage
        $this.BranchTokenStages = [System.Collections.Generic.Dictionary[string, Stage]]::new()
        $this.BrandingTokenStages = [System.Collections.Generic.Dictionary[string, Stage]]::new()
        $this.DisabledReleaseToken = $Null -Ne $entry.alwaysDisabledReleaseTokens

        ForEach ($b in $entry.alwaysDisabledBranchTokens.branchToken) {
            $this.BranchTokenStages[$b] = [Stage]::AlwaysDisabled
        }

        # AlwaysEnabled branches win over AlwaysDisabled branches
        ForEach ($b in $entry.alwaysEnabledBranchTokens.branchToken) {
            $this.BranchTokenStages[$b] = [Stage]::AlwaysEnabled
        }

        ForEach ($b in $entry.alwaysDisabledBrandingTokens.brandingToken) {
            $this.BrandingTokenStages[$b] = [Stage]::AlwaysDisabled
        }

        # AlwaysEnabled brandings win over AlwaysDisabled brandings
        ForEach ($b in $entry.alwaysEnabledBrandingTokens.brandingToken) {
            $this.BrandingTokenStages[$b] = [Stage]::AlwaysEnabled
        }
    }

    [string] PreprocessorName() {
        return "TIL_$($this.Name.ToUpper())_ENABLED"
    }
}

class FeatureComparer : System.Collections.Generic.IComparer[Feature] {
    [int] Compare([Feature]$a, [Feature]$b) {
        If ($a.Name -lt $b.Name) {
            Return -1
        } ElseIf ($a.Name -gt $b.Name) {
            Return 1
        } Else {
            Return 0
        }
    }
}

Function Resolve-FinalFeatureStage {
    Param(
        [Feature]$Feature,
        [string]$Branch,
        [string]$Branding
    )

    # RELEASE=DISABLED wins all checks
    # Then, branch match by most-specific branch
    # Then, branding type (if no overriding branch match)

    If ($Branding -Eq "Release" -And $Feature.DisabledReleaseToken) {
        [Stage]::AlwaysDisabled
        Return
    }

    If (-Not [String]::IsNullOrEmpty($Branch)) {
        $lastMatchLen = 0
        $branchStage = $Null
        ForEach ($branchToken in $Feature.BranchTokenStages.Keys) {
            # Match the longest branch token -- it should be the most specific
            If ($Branch -Like $branchToken -And $branchToken.Length -Gt $lastMatchLen) {
                $lastMatchLen = $branchToken.Length
                $branchStage = $Feature.BranchTokenStages[$branchToken]
            }
        }
        If ($Null -Ne $branchStage) {
            $branchStage
            Return
        }
    }

    $BrandingStage = $Feature.BrandingTokenStages[$Branding]
    If ($Null -Ne $BrandingStage) {
        $BrandingStage
        Return
    }

    $Feature.Stage
}

$ErrorActionPreference = "Stop"
$x = [xml](Get-Content $Path -EA:Stop)
$x.Schemas.Add('http://microsoft.com/TilFeatureStaging-Schema.xsd', (Resolve-Path (Join-Path $PSScriptRoot "FeatureStagingSchema.xsd")).Path) | Out-Null
$x.Validate($null)

$featureComparer = [FeatureComparer]::new()
$features = [System.Collections.Generic.List[Feature]]::new(16)

ForEach ($entry in $x.featureStaging.feature) {
    $features.Add([Feature]::new($entry))
}

$features.Sort($featureComparer)

$featureFinalStages = [System.Collections.Generic.Dictionary[string, Stage]]::new(16)

$branch = $BranchOverride
If ([String]::IsNullOrEmpty($branch)) {
    Try {
        $branch = & git branch --show-current 2>$Null
    } Catch {
        Try {
            $branch = & git rev-parse --abbrev-ref HEAD 2>$Null
        } Catch {
            Write-Verbose "Cannot determine current Git branch; skipping branch validation"
        }
    }
}

ForEach ($feature in $features) {
    $featureFinalStages[$feature.Name] = Resolve-FinalFeatureStage -Feature $feature -Branch $branch -Branding $Branding
}

### CODE GENERATION

$script:Output = ""
Function AddOutput($s) {
    $script:Output += $s
}

AddOutput @"
// THIS FILE IS AUTOMATICALLY GENERATED; DO NOT EDIT IT
// INPUT FILE: $Path

"@

ForEach ($feature in $features) {
    $stage = $featureFinalStages[$feature.Name]

    AddOutput @"
#define $($feature.PreprocessorName()) $(If ($stage -eq [Stage]::AlwaysEnabled) { "1" } Else { "0" })

"@
}

AddOutput @"

#if defined(__cplusplus)

"@

ForEach ($feature in $features) {
    AddOutput @"
__pragma(detect_mismatch("ODR_violation_$($feature.PreprocessorName())_mismatch", "$($feature.Stage)"))
struct $($feature.Name)
{
    static constexpr bool IsEnabled() { return $($feature.PreprocessorName()) == 1; }
};

"@
}

AddOutput @"
#endif
"@

If ([String]::IsNullOrEmpty($OutputPath)) {
    $script:Output
} Else {
    Out-File -Encoding UTF8 -FilePath $OutputPath -InputObject $script:Output
}
