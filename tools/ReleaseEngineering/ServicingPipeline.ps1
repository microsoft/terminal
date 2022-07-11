# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
#Requires -Modules PSGitHub

[CmdletBinding()]
Param(
    [string]$Version,
    [string]$SourceBranch = "origin/main",
    [switch]$GpgSign = $False
)

Function Prompt() {
    "PSCR {0}> " -f $PWD.Path
}

Function Enter-ConflictResolutionShell($entry) {
    $Global:Abort = $False
    $Global:Skip = $False
    $Global:Reject = $False
    Push-Location -StackName:"ServicingStack" -Path:$PWD > $Null
    Write-Host (@"
`e[31;1;7mCONFLICT RESOLUTION REQUIRED`e[m
`e[1mCommit `e[m: `e[1;93m{0}`e[m
`e[1mSubject`e[m: `e[1;93m{1}`e[m

"@ -f ($_.CommitID, $_.Subject))

    & git status --short

    Write-Host (@"

`e[1mCommands`e[m
`e[1;96mdone  `e[m Complete conflict resolution and commit
`e[1;96mskip  `e[m Skip this commit
`e[1;96mabort `e[m Stop everything
`e[1;96mreject`e[m Skip and `e[31mremove this commit from servicing consideration`e[m
"@)
    $Host.EnterNestedPrompt()
    Pop-Location -StackName:"ServicingStack" > $Null
}

Function Done() {
    $Host.ExitNestedPrompt()
}

Function Abort() {
    $Global:Abort = $True
    $Host.ExitNestedPrompt()
}

Function Skip() {
    $Global:Skip = $True
    $Host.ExitNestedPrompt()
}

Function Reject() {
    $Global:Skip = $True
    $Global:Reject = $True
    $Host.ExitNestedPrompt()
}

If ([String]::IsNullOrEmpty($Version)) {
    $BranchVersionRegex = [Regex]"^release-(\d+(\.\d+)+)$"
    $Branch = & git rev-parse --abbrev-ref HEAD
    $Version = $BranchVersionRegex.Match($Branch).Groups[1].Value
    If ([String]::IsNullOrEmpty($Version)) {
        Write-Error "No version specified, and we can't infer it from the name of your branch ($Branch)."
        Exit 1
    }
    Write-Host "Inferred servicing version $Version"
}

$Script:TodoColumnName = "To Cherry Pick"
$Script:DoneColumnName = "Cherry Picked"
$Script:RejectColumnName = "Rejected"

# Propagate default values into all PSGitHub cmdlets
$PSDefaultParameterValues['*GitHub*:Owner'] = "microsoft"
$PSDefaultParameterValues['*GitHub*:RepositoryName'] = "terminal"

$Project = Get-GithubProject -Name "$Version Servicing Pipeline"
$AllColumns = Get-GithubProjectColumn -ProjectId $Project.id
$ToPickColumn = $AllColumns | ? Name -Eq $script:TodoColumnName
$DoneColumn = $AllColumns | ? Name -Eq $script:DoneColumnName
$RejectColumn = $AllColumns | ? Name -Eq $script:RejectColumnName
$Cards = Get-GithubProjectCard -ColumnId $ToPickColumn.id

& git fetch --all 2>&1 | Out-Null

$Entries = @(& git log $SourceBranch --grep "(#\($($Cards.Number -Join "\|")\))" "--pretty=format:%H%x1C%s"  |
    ConvertFrom-CSV -Delimiter "`u{001C}" -Header CommitID,Subject)

[Array]::Reverse($Entries)

$PickArgs = @()
If ($GpgSign) {
    $PickArgs += , "-S"
}

$CommitPRRegex = [Regex]"\(#(\d+)\)$"
$Entries | ForEach-Object {
    Write-Host "`e[96m`e[1;7mPICK`e[22;27m $($_.CommitID): $($_.Subject)`e[m"
    $PR = $CommitPRRegex.Match($_.Subject).Groups[1].Value
    $Card = $Cards | ? Number -Eq $PR
    $null = & git cherry-pick -x $_.CommitID 2>&1
    $Err = ""
    While ($True) {
        If ($LASTEXITCODE -ne 0) {
            $Err
            Enter-ConflictResolutionShell $_

            If ($Global:Abort) {
                & git cherry-pick --abort
                Write-Host -ForegroundColor "Red" "YOU'RE ON YOUR OWN"
                Exit
            }

            If ($Global:Reject) {
                Move-GithubProjectCard -CardId $Card.id -ColumnId $RejectColumn.id
                # Fall through to Skip
            }

            If ($Global:Skip) {
                & git cherry-pick --skip
                Break
            }

            $Err = & git cherry-pick --continue --no-edit
        } Else {
            & git commit @PickArgs --amend --no-edit --trailer "Service-Card-Id:$($Card.Id)" --trailer "Service-Version:$Version" | Out-Null
            Write-Host "`e[92;1;7m OK `e[m"
            Move-GithubProjectCard -CardId $Card.id -ColumnId $DoneColumn.id
            Break
        }
    }
}
