# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

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
    [switch]$NoOverrides, # Do not include overrides
    [switch]$Full = $False # Include Narrow codepoints
)

Enum CodepointWidth {
    Narrow;
    Wide;
    Ambiguous;
    Invalid;
}

# UCD Functions {{{
Function Get-UCDEntryRange($entry) {
    $s = $e = 0
    if ($null -Ne $v.cp) {
        # Individual Codepoint
        $s = $e = [int]("0x"+$v.cp)
    } ElseIf ($null -Ne $v."first-cp") {
        # Range of Codepoints
        $s = [int]("0x"+$v."first-cp")
        $e = [int]("0x"+$v."last-cp")
    }
    $s
    $e
}

Function Get-UCDEntryWidth($entry) {
    If ($entry.Emoji -Eq "Y" -And $entry.EPres -Eq "Y") {
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
    }
    [CodepointWidth]::Invalid
}

Function Get-UCDEntryFlags($entry) {
    If ($script:Pack) {
        # If we're "pack"ing entries, only the width matters for telling them apart
        Get-UCDEntryWidth $entry
        Return
    }

    $normalizedEAWidth = $entry.ea
    $normalizedEAWidth = $normalizedEAWidth -Eq "F" ? "W" : $normalizedEAWidth;
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

        If (-Not $script:Pack -And $ucdEntry.Emoji -Eq "Y" -And $ucdEntry.EPres -Eq "Y") {
            $this.Comment = "Emoji=Y EPres=Y"
        }

        If ($null -Ne $ucdEntry.comment) {
            $this.Comment = $ucdEntry.comment
        }
    }

    [int] CompareTo([object]$Other) {
        If ($Other -Is [int]) {
            Return $this.Start - $Other
        }
        Return $this.Start - $Other.Start
    }

    [bool] Merge([UnicodeRange]$Other) {
        # If there's more than one codepoint between them, don't merge
        If (($Other.Start - $this.End) -Gt 1) {
            Return $false
        }

        # Flags are different: do not merge
        If ($this.Flags -Ne $Other.Flags) {
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
        If ($l -Lt 0) {
            # Return value <0: value was not found, return value is bitwise complement the index of the first >= value
            Return -Bnot $l
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
        If($i -Gt 0 -And $this[$i - 1].End -Ge $newRange.Start) {
            $prev = $i - 1
        }

        # Right overlap can be Infinite (because we didn't account for End)
        # Find extent of right overlap
        For($next = $i; ($next -Lt $this.Count - 1) -And ($this[$next+1].Start -Le $newRange.End); $next++) { }
        If ($this[$next].Start -Gt $newRange.End) {
            # It turns out we didn't damage the following range; clear it
            $next = $null
        }

        If ($null -Ne $next) {
            # Replace damaged elements after I with a truncated range
            $last = $this[$next]
            $this.RemoveRange($i, $next - $i + 1) # Remove damaged elements after I
            $last.Start = $newRange.End + 1
            If ($last.Start -Le $last.End) {
                $subset.Add($last)
            }
        }

        If ($null -Ne $prev) {
            # Replace damaged elements before I with a truncated range
            $first = $this[$prev]
            $this.RemoveRange($prev, $i - $prev) # Remove damaged elements (b/c we may not need to re-add them!)
            $first.End = $newRange.Start - 1
            If ($first.End -Ge $first.Start) {
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
    if ($null -Ne $_.cp) {
        [int]("0x"+$_.cp)
    } ElseIf ($null -Ne $_."first-cp") {
        [int]("0x"+$_."first-cp")
    }
}

If (-Not $Full) {
    $UCDRepertoire = $UCDRepertoire | Where-Object {
        # Select everything Wide/Ambiguous/Full OR Emoji w/ Emoji Presentation
        ($_.ea -NotIn "N", "Na", "H") -Or ($_.Emoji -Eq "Y" -And $_.EPres -Eq "Y")
    }
}

$ranges = [UnicodeRangeList]::New(1024)

$c = 0
ForEach($v in $UCDRepertoire) {
    $range = [UnicodeRange]::new($v)
    $c += $range.Length()

    If ($ranges.Count -Gt 0 -and $ranges[$ranges.Count - 1].Merge($range)) {
        # Merged into last entry
        Continue
    }
    $ranges.Add([object]$range)
}

If (-Not $NoOverrides) {
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

# Emit Code
"    // Generated by {0} -Pack:{1} -Full:{2} -NoOverrides:{3}" -f $MyInvocation.MyCommand.Name, $Pack, $Full, $NoOverrides
"    // on {0} (UTC) from {1}." -f (Get-Date -AsUTC), $InputObject.ucd.description
"    // {0} (0x{0:X}) codepoints covered." -f $c
If (-Not $NoOverrides) {
"    // {0} (0x{0:X}) codepoints overridden." -f $overrideCount
}
"    static constexpr std::array<UnicodeRange, {0}> s_wideAndAmbiguousTable{{" -f $ranges.Count
ForEach($_ in $ranges) {
    $comment = ""
    if ($null -Ne $_.Comment) {
        # We only vend comments when we aren't packing tightly
        $comment = " // {0}" -f $_.Comment
    }
"        UnicodeRange{{ 0x{0:x}, 0x{1:x}, CodepointWidth::{2} }},{3}" -f $_.Start, $_.End, $_.Width, $comment
}
"    };"
