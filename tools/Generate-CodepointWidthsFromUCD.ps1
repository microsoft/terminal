# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
# (we use the null coalescing operator)

################################################################################
# This script generates the an array suitable for replacing the body of
# src/types/CodepointWidthDetector.cpp from a Unicode UCD XML document[1]
# compliant with UAX#42[2].
#
# This script supports a quasi-mandatory "overrides" file, overrides.xml.
# If you do not have overrides, supply the -NoOverrides parameter. This was
# developed for use with the CodepointWidthDetector, which has some override
# ranges.
#
# This script was developed against the flat "no han unification" UCD
# "ucd.nounihan.flat.xml".
# It does not support the grouped database format.
# significantly smaller, which would provide a performance win on the admittedly
# extremely rare occasion that we should need to regenerate our table.
#
# Invoke this script from the root of this repository as:
#   .\tools\Generate-CodepointWidthsFromUCD.ps1 -Path .\path\to\ucd.nounihan.flat.xml -OverridePath .\src\types\unicode_width_overrides.xml -Pack
#
# [1]: https://www.unicode.org/Public/UCD/latest/ucdxml/
# [2]: https://www.unicode.org/reports/tr42/

[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidUsingPositionalParameters', '')]
[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSUseProcessBlockForPipelineCommand', '')]
[CmdletBinding()]
Param(
    [Parameter(Position=0, ValueFromPipeline=$true, ParameterSetName="Parsed")]
    [System.Xml.XmlDocument]$InputObject,

    [Parameter(Position=1, ValueFromPipeline=$true, ParameterSetName="Parsed")]
    [System.Xml.XmlDocument]$OverrideObject,

    [Parameter(Position=0, ValueFromPipelineByPropertyName=$true, ParameterSetName="Unparsed")]
    [string]$Path = "ucd.nounihan.flat.xml",

    [Parameter(Position=1, ValueFromPipelineByPropertyName=$true, ParameterSetName="Unparsed")]
    [string]$OverridePath = "overrides.xml",

    [switch]$Pack, # Pack tightly based on width
    [switch]$NoOverrides # Do not include overrides
)

Enum CodepointWidth {
    Narrow;
    Wide;
    Ambiguous;
}

# UCD Functions {{{
Function Get-UCDEntryRange($entry) {
    $s = $e = 0
    if ($null -ne $v.cp) {
        # Individual Codepoint
        $s = $e = [int]("0x"+$v.cp)
    } ElseIf ($null -ne $v."first-cp") {
        # Range of Codepoints
        $s = [int]("0x"+$v."first-cp")
        $e = [int]("0x"+$v."last-cp")
    }
    $s
    $e
}

Function Get-UCDEntryWidth($entry) {
    If ($entry.Emoji -eq "Y" -and $entry.EPres -eq "Y") {
        [CodepointWidth]::Wide
        Return
    }

    Switch($entry.ea) {
        "N"  { [CodepointWidth]::Narrow; Return }
        "Na" { [CodepointWidth]::Narrow; Return }
        "H"  { [CodepointWidth]::Narrow; Return }
        "W"  { [CodepointWidth]::Wide; Return }
        "F"  { [CodepointWidth]::Wide; Return }
        "A"  { [CodepointWidth]::Ambiguous; Return }
        default { throw "Unexpected East_Asian_Width property" }
    }
}

Function Get-UCDEntryFlags($entry) {
    If ($script:Pack) {
        # If we're "pack"ing entries, only the computed width matters for telling them apart
        Get-UCDEntryWidth $entry
        Return
    }

    $normalizedEAWidth = $entry.ea
    $normalizedEAWidth = $normalizedEAWidth -eq "F" ? "W" : $normalizedEAWidth;
    "{0}{1}{2}" -f $normalizedEAWidth, $entry.Emoji, $entry.EPres
}
# }}}

Class UnicodeRange : System.IComparable {
    [int]$Start
    [int]$End
    [CodepointWidth]$Width
    [string]$Flags
    [string]$Comment

    UnicodeRange([System.Xml.XmlElement]$ucdEntry) {
        $this.Start, $this.End = Get-UCDEntryRange $ucdEntry
        $this.Width = Get-UCDEntryWidth $ucdEntry
        $this.Flags = Get-UCDEntryFlags $ucdEntry

        If (-not $script:Pack -and $ucdEntry.Emoji -eq "Y" -and $ucdEntry.EPres -eq "Y") {
            $this.Comment = "Emoji=Y EPres=Y"
        }

        If ($null -ne $ucdEntry.comment) {
            $this.Comment = $ucdEntry.comment
        }
    }

    [int] CompareTo([object]$Other) {
        If ($Other -is [int]) {
            Return $this.Start - $Other
        }
        Return $this.Start - $Other.Start
    }

    [bool] Merge([UnicodeRange]$Other) {
        # If there's more than one codepoint between them, don't merge
        If (($Other.Start - $this.End) -gt 1) {
            Return $false
        }

        # Comments are different: do not merge
        If ($this.Comment -ne $Other.Comment) {
            Return $false
        }

        # Flags are different: do not merge
        If ($this.Flags -ne $Other.Flags) {
            Return $false
        }

        $this.End = $Other.End
        Return $true
    }

    [int] Length() {
        return $this.End - $this.Start + 1
    }
}

Class UnicodeRangeList : System.Collections.Generic.List[Object] {
    UnicodeRangeList([int]$Capacity) : base($Capacity) { }

    [int] hidden _FindInsertionPoint([int]$codepoint) {
        $l = $this.BinarySearch($codepoint)
        If ($l -lt 0) {
            # Return value <0: value was not found, return value is bitwise complement the index of the first >= value
            Return -bNOT $l
        }
        Return $l
    }

    ReplaceUnicodeRange([UnicodeRange]$newRange) {
        $subset = [System.Collections.Generic.List[Object]]::New(3)
        $subset.Add($newRange)

        $i = $this._FindInsertionPoint($newRange.Start)

        # Left overlap can only ever be one (_FindInsertionPoint always returns the
        # index immediately after the range whose Start is <= than ours).
        $prev = $null
        If($i -gt 0 -and $this[$i - 1].End -ge $newRange.Start) {
            $prev = $i - 1
        }

        # Right overlap can be Infinite (because we didn't account for End)
        # Find extent of right overlap
        For($next = $i; ($next -lt $this.Count - 1) -and ($this[$next+1].Start -le $newRange.End); $next++) { }
        If ($this[$next].Start -gt $newRange.End) {
            # It turns out we didn't damage the following range; clear it
            $next = $null
        }

        If ($null -ne $next) {
            # Replace damaged elements after I with a truncated range
            $last = $this[$next]
            $this.RemoveRange($i, $next - $i + 1) # Remove damaged elements after I
            $last.Start = $newRange.End + 1
            If ($last.Start -le $last.End) {
                $subset.Add($last)
            }
        }

        If ($null -ne $prev) {
            # Replace damaged elements before I with a truncated range
            $first = $this[$prev]
            $this.RemoveRange($prev, $i - $prev) # Remove damaged elements (b/c we may not need to re-add them!)
            $first.End = $newRange.Start - 1
            If ($first.End -ge $first.Start) {
                $subset.Insert(0, $first)
            }
            $i = $prev # Update the insertion cursor
        }

        $this.InsertRange($i, $subset)
    }
}

# Ingest UCD
If ($null -eq $InputObject) {
    $InputObject = [xml](Get-Content $Path)
}

$UCDRepertoire = $InputObject.ucd.repertoire.ChildNodes | Sort-Object {
    # Sort by either cp or first-cp (for ranges)
    if ($null -ne $_.cp) {
        [int]("0x"+$_.cp)
    } ElseIf ($null -ne $_."first-cp") {
        [int]("0x"+$_."first-cp")
    }
}

$ranges = [UnicodeRangeList]::New(1024)

ForEach($v in $UCDRepertoire) {
    $range = [UnicodeRange]::new($v)
    If ($ranges.Count -gt 0 -and $ranges[$ranges.Count - 1].Merge($range)) {
        # Merged into last entry
        Continue
    }
    $ranges.Add([object]$range)
}

If (-not $NoOverrides) {
    If ($null -eq $OverrideObject) {
        $OverrideObject = [xml](Get-Content $OverridePath)
    }

    $OverrideRepertoire = $OverrideObject.ucd.repertoire.ChildNodes
    $overrideCount = 0
    ForEach($v in $OverrideRepertoire) {
        $range = [UnicodeRange]::new($v)
        $overrideCount += $range.Length()
        $range.Comment = $range.Comment ?? "overridden without comment"
        $ranges.ReplaceUnicodeRange($range)
    }
}

$ranges.RemoveAll({ $args[0].Width -eq [CodepointWidth]::Narrow }) | Out-Null

$c = 0
ForEach($_ in $ranges) {
    $c += $_.End - $_.Start + 1
}

# Emit Code
"    // Generated by {0} -Pack:{1} -Full:{2} -NoOverrides:{3}" -f $MyInvocation.MyCommand.Name, $Pack, $Full, $NoOverrides
"    // on {0} from {1}." -f (Get-Date -AsUTC -Format "u"), $InputObject.ucd.description
"    // {0} (0x{0:X}) codepoints covered." -f $c
If (-not $NoOverrides) {
"    // {0} (0x{0:X}) codepoints overridden." -f $overrideCount
"    // Override path: {0}" -f $OverridePath
}
"    static constexpr std::array<UnicodeRange, {0}> s_wideAndAmbiguousTable{{" -f $ranges.Count
ForEach($_ in $ranges) {
    $isAmbiguous = $_.Width -eq [CodepointWidth]::Ambiguous
    $comment = ""
    if ($null -ne $_.Comment) {
        # We only vend comments when we aren't packing tightly
        $comment = " // {0}" -f $_.Comment
    }
"        UnicodeRange{{ 0x{0:x}, 0x{1:x}, {2} }},{3}" -f $_.Start, $_.End, [int]$isAmbiguous, $comment
}
"    };"
