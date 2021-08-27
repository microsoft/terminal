# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#################################
# New-TerminalStackedChangelog generates a markdown file with attribution
# over a set of revision ranges.
# Dustin uses it when he's writing changelogs.
# 
## For example, generating the changelogs for both 1.9 and 1.10 might look like this:
# $ New-TerminalStackedChangelog 1.9..release-1.9 1.10..release-1.10
## The output will be a markdown-like document. Each commit will be converted into a
## single-line entry with attribution and an optional count:
#
# * Frob the foo (thanks @PanosP!)
# * [2] Fix the bug
#
# Entries with a count were present in both changelists.

[CmdletBinding()]
Param(
    [string[]]$RevisionRanges
)

Function Test-MicrosoftPerson($email) {
    Return $email -like "*@microsoft.com" -Or $email -like "pankaj.d*"
}

Function Generate-Thanks($Entry) {
	If ($_.Microsoft) {
		""
	} ElseIf (-Not [string]::IsNullOrEmpty($_.PossibleUsername)) {
		" (thanks @{0}!)" -f $_.PossibleUsername
	} Else {
		" (thanks @<{0}>!)" -f $_.Email
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

$Entries = @()

ForEach ($RevisionRange in $RevisionRanges) {
	$NewEntries = & git log $RevisionRange "--pretty=format:%an%x1C%ae%x1C%s" |
	    ConvertFrom-CSV -Delimiter "`u{001C}" -Header Author,Email,Subject

	$Entries += $NewEntries | % { [PSCustomObject]@{
		Author = $_.Author;
		Email = $_.Email;
		Subject = $_.Subject;
		Microsoft = (Test-MicrosoftPerson $_.Email);
		PossibleUsername = (Get-PossibleUserName $_.Email);
	} }
}

$Uniq = $Entries | Group-Object Subject | %{ $_.Group[0] | Add-Member Count $_.Count -Force -PassThru }

$Uniq | % {
	$c = ""
	If ($_.Count -Gt 1) {
		$c = "[{0}] " -f $_.Count
	}
	"* {0}{1}{2}" -f ($c, $_.Subject, (Generate-Thanks $_))
}
