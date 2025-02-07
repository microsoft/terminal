# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#################################
# New-TerminalStackedChangelog generates a markdown file with attribution
# over a set of revision ranges.
# Dustin uses it when he's writing the changelog.
# 
## For example, generating the changelog for both 1.9 and 1.10 might look like this:
# $ New-TerminalStackedChangelog 1.9..release-1.9, 1.10..release-1.10
## The output will be a markdown-like document. Each commit will be converted into a
## single-line entry with attribution and an optional count:
#
# * Foo the bar (thanks @PanosP!)
# * [2] Fix the bug
#
# Entries with a count were present in both changelists.
#
# If you don't have the release tags/branches checked out locally, you might
# need to do something like:
#
# $ New-TerminalStackedChangelog origin/release-1.8..origin/release-1.9

#Requires -Version 7.0

[CmdletBinding()]
Param(
    [string[]]$RevisionRanges,
    [string]$Clog
)

Function Test-MicrosoftPerson($email) {
    Return $email -like "*@microsoft.com" -Or $email -like "pankaj.d*"
}

Function Generate-Thanks($Entry) {
    # We don't need to thank ourselves for doing our jobs
    If ($Entry.Microsoft) {
        ""
    } ElseIf (-Not [string]::IsNullOrEmpty($Entry.PossibleUsername)) {
        " (thanks @{0}!)" -f $Entry.PossibleUsername
    } Else {
        " (thanks @<{0}>!)" -f $Entry.Email
    }
}

$usernameRegex = [regex]::new("(?:\d+\+)?(?<name>[^@]+)@users.noreply.github.com")
Function Get-PossibleUserName($email) {
    $match = $usernameRegex.Match($email)
    if ($null -Ne $match) {
        return $match.Groups["name"].Value
    }
    return $null
}

$cll=gc $Clog | convertfrom-json -depth 99
$prr=[regex]::new('\(#(\d+)\)$')

$Entries = @()

$i = 0
ForEach ($RevisionRange in $RevisionRanges) {
    # --pretty=format notes:
    #   - %an: author name
    #   - %x1C: print a literal FS (0x1C), which will be used as a delimiter
    #   - %ae: author email
    #   - %x1C: another FS
    #   - %s: subject, the title of the commit
    $NewEntries = & git log $RevisionRange --first-parent "--pretty=format:%H%x1C%an%x1C%ae%x1C%s" |
        ConvertFrom-CSV -Delimiter "`u{001C}" -Header SHA,Author,Email,Subject

    $Entries += $NewEntries | % { 
        $id = $_.SHA
        $maa=$prr.Match($_.Subject)
        $pr = ""
        if ($maa.Success) {
            $id = "pr-"+$maa.Groups[1].Value
            $pr = " (#$($maa.Groups[1].Value))"
        }

        $cle = $cll.$id
        if ($null -ne $cle) {
            if ($cle.Ignore) {
                return
            }
            $slug = switch($cle.Type) {
                1 { "b" }
                2 { "c" }
                3 { "f" }
                default { "x" }
            }
            if ($cle.Priority -gt 0) {
                $slug += ("{0}" -f $cle.Priority)
            }
            $_.Subject = $slug + " " + $cle.Title + $pr
        } Else {
            $_.Subject = "%%% "+$_.Subject
        }

        [PSCustomObject]@{
            ID = $_.ID;
            Author = $_.Author;
            Email = $_.Email;
            Subject = $_.Subject;
            Microsoft = (Test-MicrosoftPerson $_.Email);
            PossibleUsername = (Get-PossibleUserName $_.Email);
            RevRangeID = $i;
        }
    }
    $i++
}

For($c = 0; $c -Lt $i; $c++) {
    "   " + ("|" * $c) + "/ " + $RevisionRanges[$c]
}

$Unique = $Entries | Group-Object Subject

$Unique | % {
    $en = $_.Group[0]
    $revSpec = (" " * $i)
    $_.Group | % {
        $revSpec = $revSpec.remove($_.RevRangeID, 1).insert($_.RevRangeID, "X")
    }
    $c = "[{0}] " -f $revSpec
    "* {0}{1}{2}" -f ($c, $en.Subject, (Generate-Thanks $en))
}
